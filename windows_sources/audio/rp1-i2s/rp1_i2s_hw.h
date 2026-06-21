/*++

Module Name:
    rp1_i2s_hw.h

Abstract:
    RP1 / Synopsys DesignWare I2S hardware abstraction layer (HAL).

    This is the *reusable* part of the port: register map + init / config /
    start / stop sequences, taken faithfully from the Linux driver
        sources/sound/soc/dwc/dwc-i2s.c
        sources/sound/soc/dwc/local.h   (GPL-2.0, ST Microelectronics / R. Kumar)

    Only the hardware logic is reused. All OS glue (readl/writel -> MMIO
    register access, clocks, DMA, IRQ) is reimplemented against the Windows
    kernel.  On Windows the MMIO accessors are READ/WRITE_REGISTER_ULONG,
    which carry the correct ARM64 device-memory ordering semantics (the
    equivalent of Linux readl()/writel()).

    This HAL is OS-policy free so it can later be driven by either the current
    KMDF skeleton (Phase 1) or a PortCls/WaveRT audio miniport (Phase 2).

--*/

#pragma once

#ifdef I2S_SIM
#include "sim/i2s_simshim.h"   /* x64 user-mode test shim */
#else
#include <ntddk.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define RP1I2S_BIT(n)                 (1UL << (n))

/* ---- DesignWare I2S register map (ported from local.h) ---- */
#define DW_I2S_IER                    0x000   /* global block enable          */
#define DW_I2S_IRER                   0x004   /* rx block enable              */
#define DW_I2S_ITER                   0x008   /* tx block enable              */
#define DW_I2S_CER                    0x00C   /* clock enable                 */
#define DW_I2S_CCR                    0x010   /* clock configuration          */
#define DW_I2S_RXFFR                  0x014   /* rx fifo reset                */
#define DW_I2S_TXFFR                  0x018   /* tx fifo reset                */

/* per-channel-pair registers, ch = 0..3 (stride 0x40) */
#define DW_I2S_LRBR_LTHR(ch)          (0x40 * (ch) + 0x020)
#define DW_I2S_RRBR_RTHR(ch)          (0x40 * (ch) + 0x024)
#define DW_I2S_RER(ch)                (0x40 * (ch) + 0x028)
#define DW_I2S_TER(ch)                (0x40 * (ch) + 0x02C)
#define DW_I2S_RCR(ch)                (0x40 * (ch) + 0x030)
#define DW_I2S_TCR(ch)                (0x40 * (ch) + 0x034)
#define DW_I2S_ISR(ch)                (0x40 * (ch) + 0x038)
#define DW_I2S_IMR(ch)                (0x40 * (ch) + 0x03C)
#define DW_I2S_ROR(ch)                (0x40 * (ch) + 0x040)
#define DW_I2S_TOR(ch)                (0x40 * (ch) + 0x044)
#define DW_I2S_RFCR(ch)               (0x40 * (ch) + 0x048)
#define DW_I2S_TFCR(ch)               (0x40 * (ch) + 0x04C)
#define DW_I2S_RFF(ch)                (0x40 * (ch) + 0x050)
#define DW_I2S_TFF(ch)                (0x40 * (ch) + 0x054)

/* enable-register bit fields */
#define DW_I2S_IER_IEN                RP1I2S_BIT(0)
#define DW_I2S_TER_TXCHEN             RP1I2S_BIT(0)
#define DW_I2S_RER_RXCHEN             RP1I2S_BIT(0)

/* interrupt status bit fields */
#define DW_I2S_ISR_TXFO               RP1I2S_BIT(5)
#define DW_I2S_ISR_TXFE               RP1I2S_BIT(4)
#define DW_I2S_ISR_RXFO               RP1I2S_BIT(1)
#define DW_I2S_ISR_RXDA               RP1I2S_BIT(0)

/* DMA data + control registers */
#define DW_I2S_TXDMA                  0x1C0
#define DW_I2S_RRXDMA                 0x1C4
#define DW_I2S_RXDMA                  0x1C8
#define DW_I2S_RTXDMA                 0x1CC
#define DW_I2S_DMACR                  0x200
#define DW_I2S_DMAEN_RXBLOCK          RP1I2S_BIT(16)
#define DW_I2S_DMAEN_TXBLOCK          RP1I2S_BIT(17)
#define DW_I2S_DMACR_DMAEN_RXCH0      RP1I2S_BIT(0)
#define DW_I2S_DMACR_DMAEN_TXCH0      RP1I2S_BIT(8)
#define DW_I2S_DMACR_DMAEN_RX         RP1I2S_BIT(16)
#define DW_I2S_DMACR_DMAEN_TX         RP1I2S_BIT(17)

/* component parameter / identity registers */
#define DW_I2S_COMP_PARAM_2           0x1F0
#define DW_I2S_COMP_PARAM_1           0x1F4
#define DW_I2S_COMP_VERSION           0x1F8
#define DW_I2S_COMP_TYPE              0x1FC
#define DW_I2S_COMP_TYPE_MAGIC        0x44571120u   /* expected DesignWare I2S id */

/* COMP_PARAM_1 / COMP_PARAM_2 field extraction (ported from local.h) */
#define DW_COMP1_TX_WORDSIZE_0(r)     (((r) >> 16) & 0x7)
#define DW_COMP1_TX_CHANNELS(r)       (((r) >>  9) & 0x3)
#define DW_COMP1_RX_CHANNELS(r)       (((r) >>  7) & 0x3)
#define DW_COMP1_RX_ENABLED(r)        (((r) >>  6) & 0x1)
#define DW_COMP1_TX_ENABLED(r)        (((r) >>  5) & 0x1)
#define DW_COMP1_MODE_EN(r)           (((r) >>  4) & 0x1)
#define DW_COMP1_FIFO_DEPTH_GLOBAL(r) (((r) >>  2) & 0x3)
#define DW_COMP1_APB_DATA_WIDTH(r)    (((r) >>  0) & 0x3)
#define DW_COMP2_RX_WORDSIZE_0(r)     (((r) >>  0) & 0x7)

/* capability flags (mirror of Linux dev->capability) */
#define DWC_I2S_PLAY                  RP1I2S_BIT(0)
#define DWC_I2S_RECORD                RP1I2S_BIT(1)
#define DW_I2S_MASTER                 RP1I2S_BIT(2)
#define DW_I2S_SLAVE                  RP1I2S_BIT(3)

typedef struct _RP1I2S_HW {
    PUCHAR  Base;            /* mapped MMIO virtual base                       */
    SIZE_T  Length;          /* mapped length                                  */
    UINT32  Comp1;           /* cached COMP_PARAM_1                            */
    UINT32  Comp2;           /* cached COMP_PARAM_2                            */
    UINT32  FifoDepth;       /* derived FIFO depth                            */
    UINT32  FifoTh;          /* FIFO threshold (= depth / 2)                  */
    UINT32  XferResolution;  /* 0x02=16-bit, 0x04=24-bit, 0x05=32-bit         */
    UINT32  Ccr;             /* CCR word-select/sclk-gate: 0x00/0x08/0x10      */
    UINT32  Capability;      /* DWC_I2S_PLAY | DWC_I2S_RECORD | MASTER/SLAVE  */
    UINT32  TxChannelsMax;
    UINT32  RxChannelsMax;
} RP1I2S_HW, *PRP1I2S_HW;

/* MMIO accessors: Windows equivalents of Linux readl()/writel(). */
FORCEINLINE UINT32 Rp1I2sRead(_In_ PRP1I2S_HW Hw, _In_ ULONG Reg)
{
    return READ_REGISTER_ULONG((volatile ULONG *)(Hw->Base + Reg));
}

FORCEINLINE VOID Rp1I2sWrite(_In_ PRP1I2S_HW Hw, _In_ ULONG Reg, _In_ UINT32 Val)
{
    WRITE_REGISTER_ULONG((volatile ULONG *)(Hw->Base + Reg), Val);
}

/* HAL entry points (ported from dwc-i2s.c) */
VOID     Rp1I2sHwInit(_Out_ PRP1I2S_HW Hw, _In_ PUCHAR Base, _In_ SIZE_T Length);
NTSTATUS Rp1I2sHwProbe(_Inout_ PRP1I2S_HW Hw);
NTSTATUS Rp1I2sHwSetResolution(_Inout_ PRP1I2S_HW Hw, _In_ UINT32 Bits);
VOID     Rp1I2sHwConfig(_Inout_ PRP1I2S_HW Hw, _In_ BOOLEAN Playback, _In_ UINT32 Channels);
VOID     Rp1I2sHwEnableIrqs(_Inout_ PRP1I2S_HW Hw, _In_ BOOLEAN Playback);
VOID     Rp1I2sHwDisableIrqs(_Inout_ PRP1I2S_HW Hw, _In_ BOOLEAN Playback);
VOID     Rp1I2sHwStart(_Inout_ PRP1I2S_HW Hw, _In_ BOOLEAN Playback);
VOID     Rp1I2sHwStop(_Inout_ PRP1I2S_HW Hw, _In_ BOOLEAN Playback);
VOID     Rp1I2sHwFlush(_Inout_ PRP1I2S_HW Hw, _In_ BOOLEAN Playback);

#ifdef __cplusplus
}
#endif
