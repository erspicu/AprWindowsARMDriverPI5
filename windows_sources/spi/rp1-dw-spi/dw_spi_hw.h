/*++
Module Name: dw_spi_hw.h
Abstract:    Synopsys DesignWare APB SSI (SPI) HAL. Reusable core ported from
             Linux drivers/spi/spi-dw.h / spi-dw-core.c. Register map + master
             configure + polled full-duplex transfer.
--*/
#pragma once
#ifdef DWSPI_SIM
#include "sim/dwspi_simshim.h"   /* x64 user-mode test shim */
#else
#include <ntddk.h>
#endif

/* RP1 DesignWare SSI functional clock = 200 MHz (clk_sys, Pi5-measured). */
#define DW_SPI_FSSI_HZ      200000000u

/* DW SSI register offsets (spi-dw.h) */
#define DW_SPI_CTRLR0       0x00
#define DW_SPI_CTRLR1       0x04
#define DW_SPI_SSIENR       0x08
#define DW_SPI_SER          0x10
#define DW_SPI_BAUDR        0x14
#define DW_SPI_TXFTLR       0x18
#define DW_SPI_RXFTLR       0x1c
#define DW_SPI_TXFLR        0x20
#define DW_SPI_RXFLR        0x24
#define DW_SPI_SR           0x28
#define DW_SPI_IMR          0x2c
#define DW_SPI_ICR          0x48
#define DW_SPI_IDR          0x58
#define DW_SPI_VERSION      0x5c
#define DW_SPI_DR           0x60

/* CTRLR0 (DWC APB SSI / PSSI) */
#define DW_PSSI_CTRLR0_DFS_SHIFT    0      /* data frame size - 1 (4-bit DFS variant) */
#define DW_PSSI_CTRLR0_DFS_MASK     0x0000000Fu   /* low 4 bits: writable iff 4-bit DFS */
#define DW_PSSI_CTRLR0_DFS32_SHIFT  16     /* extended (DFS32) variant puts DFS at [20:16] */
#define DW_PSSI_CTRLR0_FRF_SHIFT    4      /* frame format (0=Motorola SPI) */
#define DW_PSSI_CTRLR0_SCPHA        (1u << 6)
#define DW_PSSI_CTRLR0_SCPOL        (1u << 7)
#define DW_PSSI_CTRLR0_TMOD_SHIFT   8
#define DW_SPI_TMOD_TR              0x0    /* transmit & receive */
#define DW_SPI_TMOD_TO             0x1    /* transmit only */
#define DW_SPI_TMOD_RO             0x2    /* receive only (needs CTRLR1 = ndf-1) */

/* SR bits */
#define DW_SPI_SR_BUSY      (1u << 0)
#define DW_SPI_SR_TFNF      (1u << 1)   /* tx fifo not full */
#define DW_SPI_SR_TFE       (1u << 2)   /* tx fifo empty */
#define DW_SPI_SR_RFNE      (1u << 3)   /* rx fifo not empty */

typedef struct _DW_SPI_HW {
    PUCHAR  Base;
    SIZE_T  Length;
    UINT32  FifoLen;
    UINT8   Bits;       /* data frame bits (default 8) */
    UINT8   Mode;       /* SPI mode 0..3 */
    UINT8   DfsOffset;  /* CTRLR0 DFS field shift: 0 (4-bit) or 16 (DFS32), auto-detected */
} DW_SPI_HW, *PDW_SPI_HW;

FORCEINLINE UINT32 DwSpiRead(_In_ PDW_SPI_HW Hw, _In_ ULONG Reg)
{ return READ_REGISTER_ULONG((volatile ULONG *)(Hw->Base + Reg)); }
FORCEINLINE VOID DwSpiWrite(_In_ PDW_SPI_HW Hw, _In_ ULONG Reg, _In_ UINT32 Val)
{ WRITE_REGISTER_ULONG((volatile ULONG *)(Hw->Base + Reg), Val); }

VOID     DwSpiHwInit(_Out_ PDW_SPI_HW Hw, _In_ PUCHAR Base, _In_ SIZE_T Length);
NTSTATUS DwSpiHwProbe(_Inout_ PDW_SPI_HW Hw);
VOID     DwSpiHwEnable(_Inout_ PDW_SPI_HW Hw, _In_ BOOLEAN Enable);
VOID     DwSpiHwResetChip(_Inout_ PDW_SPI_HW Hw);   /* dw_spi_reset_chip */
UINT16   DwSpiHwBaudDiv(_In_ UINT32 SclkHz);   /* SCKDV from target SCLK @ fssi=200MHz */
VOID     DwSpiHwConfigureMaster(_Inout_ PDW_SPI_HW Hw, _In_ UINT8 Bits, _In_ UINT8 Mode, _In_ UINT16 BaudDiv);
VOID     DwSpiHwSetCs(_Inout_ PDW_SPI_HW Hw, _In_ ULONG Cs, _In_ BOOLEAN Assert);
NTSTATUS DwSpiHwTransferPolled(_Inout_ PDW_SPI_HW Hw,
                               _In_reads_opt_(Len) const UCHAR *Tx,
                               _Out_writes_opt_(Len) UCHAR *Rx, _In_ ULONG Len);
