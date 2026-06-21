/*++

Module Name:
    rp1_i2s_hw.c

Abstract:
    RP1 / Synopsys DesignWare I2S hardware abstraction layer (HAL).

    Faithful port of the *hardware logic* from Linux:
        sources/sound/soc/dwc/dwc-i2s.c  (functions dw_i2s_config, i2s_start,
                                           i2s_stop, dw_i2s_hw_params, dw_configure_dai)
    Linux readl()/writel() -> Rp1I2sRead()/Rp1I2sWrite() (READ/WRITE_REGISTER_ULONG).
    No OS policy here (no clocks / DMA / IRQ wiring) - that belongs to the driver
    layer (Phase 1: WDM skeleton; Phase 2: PortCls/WaveRT + DMA + codec).

--*/

#include "rp1_i2s_hw.h"

/* ported from i2s_disable_channels() */
static VOID Rp1I2sDisableChannels(_Inout_ PRP1I2S_HW Hw, _In_ BOOLEAN Playback)
{
    UINT32 i;
    for (i = 0; i < 4; i++) {
        Rp1I2sWrite(Hw, Playback ? DW_I2S_TER(i) : DW_I2S_RER(i), 0);
    }
}

VOID Rp1I2sHwInit(_Out_ PRP1I2S_HW Hw, _In_ PUCHAR Base, _In_ SIZE_T Length)
{
    RtlZeroMemory(Hw, sizeof(*Hw));
    Hw->Base = Base;
    Hw->Length = Length;
    Hw->XferResolution = 0x02; /* default 16-bit */
}

/*
 * ported from dw_configure_dai() / dw_configure_dai_by_dt():
 * read the component parameter registers to learn play/record capability,
 * channel counts, FIFO depth and master/slave mode.
 */
NTSTATUS Rp1I2sHwProbe(_Inout_ PRP1I2S_HW Hw)
{
    UINT32 comp1, comp2, type;

    type  = Rp1I2sRead(Hw, DW_I2S_COMP_TYPE);
    comp1 = Rp1I2sRead(Hw, DW_I2S_COMP_PARAM_1);
    comp2 = Rp1I2sRead(Hw, DW_I2S_COMP_PARAM_2);

    Hw->Comp1 = comp1;
    Hw->Comp2 = comp2;
    Hw->FifoDepth = 1u << (1 + DW_COMP1_FIFO_DEPTH_GLOBAL(comp1));
    Hw->FifoTh = Hw->FifoDepth / 2;
    Hw->Capability = 0;

    if (DW_COMP1_TX_ENABLED(comp1)) {
        Hw->Capability |= DWC_I2S_PLAY;
        Hw->TxChannelsMax = 2 * (DW_COMP1_TX_CHANNELS(comp1) + 1);
    }
    if (DW_COMP1_RX_ENABLED(comp1)) {
        Hw->Capability |= DWC_I2S_RECORD;
        Hw->RxChannelsMax = 2 * (DW_COMP1_RX_CHANNELS(comp1) + 1);
    }
    Hw->Capability |= DW_COMP1_MODE_EN(comp1) ? DW_I2S_MASTER : DW_I2S_SLAVE;

    /* If MMIO is bogus (e.g. skeleton load with no real RP1), bail out cleanly. */
    if (type != DW_I2S_COMP_TYPE_MAGIC) {
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }
    return STATUS_SUCCESS;
}

/* ported from dw_i2s_hw_params() resolution mapping: each format sets BOTH the
 * per-channel xfer_resolution (TCR/RCR) AND the CCR word-select/sclk-gate code. */
NTSTATUS Rp1I2sHwSetResolution(_Inout_ PRP1I2S_HW Hw, _In_ UINT32 Bits)
{
    switch (Bits) {
    case 16: Hw->XferResolution = 0x02; Hw->Ccr = 0x00; break;
    case 24: Hw->XferResolution = 0x04; Hw->Ccr = 0x08; break;
    case 32: Hw->XferResolution = 0x05; Hw->Ccr = 0x10; break;
    default: return STATUS_INVALID_PARAMETER;
    }
    return STATUS_SUCCESS;
}

/* faithful port of dw_i2s_config() */
VOID Rp1I2sHwConfig(_Inout_ PRP1I2S_HW Hw, _In_ BOOLEAN Playback, _In_ UINT32 Channels)
{
    UINT32 dmacr;
    UINT32 pairs;
    UINT32 ch;

    Rp1I2sDisableChannels(Hw, Playback);

    dmacr = Rp1I2sRead(Hw, DW_I2S_DMACR);
    if (Playback) {
        dmacr &= ~(DW_I2S_DMACR_DMAEN_TXCH0 * 0xf);
    } else {
        dmacr &= ~(DW_I2S_DMACR_DMAEN_RXCH0 * 0xf);
    }

    pairs = Channels / 2;
    for (ch = 0; ch < pairs && ch < 4; ch++) {
        if (Playback) {
            Rp1I2sWrite(Hw, DW_I2S_TCR(ch), Hw->XferResolution);
            Rp1I2sWrite(Hw, DW_I2S_TFCR(ch), Hw->FifoDepth - Hw->FifoTh - 1);
            Rp1I2sWrite(Hw, DW_I2S_TER(ch), DW_I2S_TER_TXCHEN);
            dmacr |= (DW_I2S_DMACR_DMAEN_TXCH0 << ch);
        } else {
            Rp1I2sWrite(Hw, DW_I2S_RCR(ch), Hw->XferResolution);
            Rp1I2sWrite(Hw, DW_I2S_RFCR(ch), Hw->FifoTh - 1);
            Rp1I2sWrite(Hw, DW_I2S_RER(ch), DW_I2S_RER_RXCHEN);
            dmacr |= (DW_I2S_DMACR_DMAEN_RXCH0 << ch);
        }
    }

    Rp1I2sWrite(Hw, DW_I2S_DMACR, dmacr);

    /* word-select size / sclk gating (dw_i2s_hw_params: i2s_write_reg(CCR, ccr)).
     * Required so BCLK frames words correctly; previously left unset. */
    Rp1I2sWrite(Hw, DW_I2S_CCR, Hw->Ccr);
}

/* Port of i2s_enable_irqs/i2s_disable_irqs: per-channel-pair IMR masking. TX uses
 * mask 0x30 (TXFE/TXFO), RX uses 0x03 (RXDA/RXFO). Applied to all 4 pairs; pairs
 * whose TER/RER are disabled never raise an IRQ, so this is safe. */
VOID Rp1I2sHwEnableIrqs(_Inout_ PRP1I2S_HW Hw, _In_ BOOLEAN Playback)
{
    UINT32 ch, mask = Playback ? 0x30u : 0x03u;
    for (ch = 0; ch < 4; ch++) {
        UINT32 irq = Rp1I2sRead(Hw, DW_I2S_IMR(ch));
        Rp1I2sWrite(Hw, DW_I2S_IMR(ch), irq & ~mask);   /* unmask */
    }
}

VOID Rp1I2sHwDisableIrqs(_Inout_ PRP1I2S_HW Hw, _In_ BOOLEAN Playback)
{
    UINT32 ch, mask = Playback ? 0x30u : 0x03u;
    for (ch = 0; ch < 4; ch++) {
        UINT32 irq = Rp1I2sRead(Hw, DW_I2S_IMR(ch));
        Rp1I2sWrite(Hw, DW_I2S_IMR(ch), irq | mask);    /* mask */
    }
}

/* faithful port of i2s_start() */
VOID Rp1I2sHwStart(_Inout_ PRP1I2S_HW Hw, _In_ BOOLEAN Playback)
{
    UINT32 dma;

    Rp1I2sWrite(Hw, DW_I2S_IER, DW_I2S_IER_IEN);
    Rp1I2sHwEnableIrqs(Hw, Playback);          /* unmask the active direction's FIFO IRQs */

    if (Playback) {
        Rp1I2sWrite(Hw, DW_I2S_ITER, 1);
    } else {
        Rp1I2sWrite(Hw, DW_I2S_IRER, 1);
    }

    /* enable the DMA block handshake (the DMA engine itself is Phase 2) */
    dma = Rp1I2sRead(Hw, DW_I2S_DMACR);
    dma |= Playback ? DW_I2S_DMAEN_TXBLOCK : DW_I2S_DMAEN_RXBLOCK;
    Rp1I2sWrite(Hw, DW_I2S_DMACR, dma);

    Rp1I2sWrite(Hw, DW_I2S_CER, 1);
}

/* port of i2s_stop() + the relevant parts of dw_i2s_shutdown() */
VOID Rp1I2sHwStop(_Inout_ PRP1I2S_HW Hw, _In_ BOOLEAN Playback)
{
    Rp1I2sHwDisableIrqs(Hw, Playback);         /* mask FIFO IRQs (i2s_stop) */
    Rp1I2sDisableChannels(Hw, Playback);

    if (Playback) {
        Rp1I2sWrite(Hw, DW_I2S_ITER, 0);
    } else {
        Rp1I2sWrite(Hw, DW_I2S_IRER, 0);
    }

    Rp1I2sWrite(Hw, DW_I2S_CER, 0);
}

/* port of dw_i2s_prepare(): reset the TX/RX FIFO */
VOID Rp1I2sHwFlush(_Inout_ PRP1I2S_HW Hw, _In_ BOOLEAN Playback)
{
    Rp1I2sWrite(Hw, Playback ? DW_I2S_TXFFR : DW_I2S_RXFFR, 1);
}
