/*++
Module Name: dw_spi_hw.c
Abstract:    DesignWare APB SSI HAL (master, polled full-duplex). Ported from
             Linux spi-dw-core.c (dw_spi_update_config / reset / transfer).
--*/
#include "dw_spi_hw.h"

#define DW_SPI_POLL_SPINS   100000

VOID DwSpiHwInit(_Out_ PDW_SPI_HW Hw, _In_ PUCHAR Base, _In_ SIZE_T Length)
{
    RtlZeroMemory(Hw, sizeof(*Hw));
    Hw->Base = Base;
    Hw->Length = Length;
    Hw->FifoLen = 16;
    Hw->Bits = 8;
}

VOID DwSpiHwEnable(_Inout_ PDW_SPI_HW Hw, _In_ BOOLEAN Enable)
{
    DwSpiWrite(Hw, DW_SPI_SSIENR, Enable ? 1 : 0);
}

/* port of dw_spi_reset_chip(): disable, mask all interrupts, read ICR to clear
 * any pending interrupt, deassert all slave selects, re-enable. Clears warm-boot
 * leftover state (stale CS / latched interrupts) that would mis-fire the 1st xfer. */
VOID DwSpiHwResetChip(_Inout_ PDW_SPI_HW Hw)
{
    DwSpiHwEnable(Hw, FALSE);
    DwSpiWrite(Hw, DW_SPI_IMR, 0);
    (void)DwSpiRead(Hw, DW_SPI_ICR);
    DwSpiWrite(Hw, DW_SPI_SER, 0);
    DwSpiHwEnable(Hw, TRUE);
}

NTSTATUS DwSpiHwProbe(_Inout_ PDW_SPI_HW Hw)
{
    UINT32 ver = DwSpiRead(Hw, DW_SPI_VERSION);
    /* A live DW SSI returns a FourCC-like version (e.g. "3.??"). Treat 0 /
     * all-ones as "no hardware" (skeleton load without real RP1 MMIO). */
    if (ver == 0 || ver == 0xffffffff) {
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }
    DwSpiHwResetChip(Hw);   /* dw_spi_hw_init calls reset on a freshly-found chip */

    /* Auto-detect the DFS field position (dw_spi_hw_init): write all-ones to CTRLR0
     * while disabled; if the low 4 bits don't stick, this is the extended-DFS (DFS32)
     * variant whose DFS field lives at [20:16]. Self-adapting -> no hardcoded guess. */
    {
        UINT32 saved, probe;
        DwSpiHwEnable(Hw, FALSE);
        saved = DwSpiRead(Hw, DW_SPI_CTRLR0);
        DwSpiWrite(Hw, DW_SPI_CTRLR0, 0xffffffffu);
        probe = DwSpiRead(Hw, DW_SPI_CTRLR0);
        DwSpiWrite(Hw, DW_SPI_CTRLR0, saved);
        DwSpiHwEnable(Hw, TRUE);
        Hw->DfsOffset = (probe & DW_PSSI_CTRLR0_DFS_MASK) ? DW_PSSI_CTRLR0_DFS_SHIFT
                                                          : DW_PSSI_CTRLR0_DFS32_SHIFT;
    }
    return STATUS_SUCCESS;
}

/* port of dw_spi_update_config(): program CTRLR0 (frame size / mode / TMOD) */
/* Compute the DesignWare SSI SCKDV divider for a target SCLK, from the RP1
 * SSI clock = 200 MHz (clk_sys, measured on a live Pi5). SCLK = fssi / SCKDV;
 * SCKDV must be even (LSB ignored by HW) and >= 2. ceil() so we never exceed
 * the requested speed. See MD/Note/20260621-0720-pi5-linux-hardware-facts.md. */
UINT16 DwSpiHwBaudDiv(_In_ UINT32 SclkHz)
{
    UINT32 div;
    if (SclkHz == 0) return 16;                          /* default ~12.5 MHz */
    div = (DW_SPI_FSSI_HZ + SclkHz - 1) / SclkHz;        /* ceil */
    if (div < 2)       div = 2;
    if (div & 1)       div++;                            /* SCKDV must be even */
    if (div > 0xFFFEu) div = 0xFFFEu;
    return (UINT16)div;
}

VOID DwSpiHwConfigureMaster(_Inout_ PDW_SPI_HW Hw, _In_ UINT8 Bits, _In_ UINT8 Mode, _In_ UINT16 BaudDiv)
{
    UINT32 ctrlr0;

    Hw->Bits = Bits ? Bits : 8;
    Hw->Mode = Mode & 0x3;

    DwSpiHwEnable(Hw, FALSE);          /* must be disabled to program */

    ctrlr0  = (UINT32)(Hw->Bits - 1) << Hw->DfsOffset;            /* DFS (auto-detected offset) */
    ctrlr0 |= 0u << DW_PSSI_CTRLR0_FRF_SHIFT;                      /* Motorola SPI */
    ctrlr0 |= (UINT32)DW_SPI_TMOD_TR << DW_PSSI_CTRLR0_TMOD_SHIFT; /* TX+RX */
    if (Hw->Mode & 0x1) ctrlr0 |= DW_PSSI_CTRLR0_SCPHA;           /* CPHA */
    if (Hw->Mode & 0x2) ctrlr0 |= DW_PSSI_CTRLR0_SCPOL;           /* CPOL */
    DwSpiWrite(Hw, DW_SPI_CTRLR0, ctrlr0);

    DwSpiWrite(Hw, DW_SPI_BAUDR, BaudDiv ? BaudDiv : 16);
    DwSpiWrite(Hw, DW_SPI_TXFTLR, 0);
    DwSpiWrite(Hw, DW_SPI_RXFTLR, 0);
    DwSpiWrite(Hw, DW_SPI_IMR, 0);    /* polled */

    DwSpiHwEnable(Hw, TRUE);
}

VOID DwSpiHwSetCs(_Inout_ PDW_SPI_HW Hw, _In_ ULONG Cs, _In_ BOOLEAN Assert)
{
    /* SER selects the active slave; clearing it deselects. */
    DwSpiWrite(Hw, DW_SPI_SER, Assert ? (1u << Cs) : 0u);
}

static NTSTATUS DwSpiWaitTxNotFull(_Inout_ PDW_SPI_HW Hw)
{
    ULONG s;
    for (s = 0; s < DW_SPI_POLL_SPINS; s++) {
        if (DwSpiRead(Hw, DW_SPI_SR) & DW_SPI_SR_TFNF) return STATUS_SUCCESS;
        KeStallExecutionProcessor(1);
    }
    return STATUS_IO_TIMEOUT;
}

static NTSTATUS DwSpiWaitRxNotEmpty(_Inout_ PDW_SPI_HW Hw)
{
    ULONG s;
    for (s = 0; s < DW_SPI_POLL_SPINS; s++) {
        if (DwSpiRead(Hw, DW_SPI_SR) & DW_SPI_SR_RFNE) return STATUS_SUCCESS;
        KeStallExecutionProcessor(1);
    }
    return STATUS_IO_TIMEOUT;
}

/* Full-duplex byte-at-a-time polled transfer (TMOD_TR). Tx/Rx may be NULL. */
NTSTATUS DwSpiHwTransferPolled(_Inout_ PDW_SPI_HW Hw,
    _In_reads_opt_(Len) const UCHAR *Tx, _Out_writes_opt_(Len) UCHAR *Rx, _In_ ULONG Len)
{
    ULONG i;
    NTSTATUS status;

    for (i = 0; i < Len; i++) {
        status = DwSpiWaitTxNotFull(Hw);
        if (!NT_SUCCESS(status)) return status;
        DwSpiWrite(Hw, DW_SPI_DR, Tx ? Tx[i] : 0xff);

        status = DwSpiWaitRxNotEmpty(Hw);
        if (!NT_SUCCESS(status)) return status;
        {
            UCHAR rb = (UCHAR)(DwSpiRead(Hw, DW_SPI_DR) & 0xff);
            if (Rx) Rx[i] = rb;
        }
    }
    return STATUS_SUCCESS;
}
