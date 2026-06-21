/*++
Module Name: bcm_spi_hw.h
Abstract:    BCM2835/BCM2712 SPI (SPI0) HAL. Reusable core ported from Linux
             drivers/spi/spi-bcm2835.c. Register map + polled full-duplex xfer.
--*/
#pragma once
#ifdef BCMSPI_SIM
#include "sim/bcmspi_simshim.h"   /* x64 user-mode test shim */
#else
#include <ntddk.h>
#endif

/* BCM2712 SPI core clock = vpu-clock = 750 MHz (Pi5-measured). */
#define BCM_SPI_CORE_HZ 750000000u

/* SPI register offsets (spi-bcm2835.c) */
#define BCM_SPI_CS      0x00
#define BCM_SPI_FIFO    0x04
#define BCM_SPI_CLK     0x08
#define BCM_SPI_DLEN    0x0c

/* CS bitfields */
#define BCM_SPI_CS_RXF       0x00100000
#define BCM_SPI_CS_RXR       0x00080000
#define BCM_SPI_CS_TXD       0x00040000
#define BCM_SPI_CS_RXD       0x00020000
#define BCM_SPI_CS_DONE      0x00010000
#define BCM_SPI_CS_TA        0x00000080   /* transfer active */
#define BCM_SPI_CS_CSPOL     0x00000040
#define BCM_SPI_CS_CLEAR_RX  0x00000020
#define BCM_SPI_CS_CLEAR_TX  0x00000010
#define BCM_SPI_CS_CPOL      0x00000008
#define BCM_SPI_CS_CPHA      0x00000004
#define BCM_SPI_CS_CS_MASK   0x00000003   /* native chip-select; 3 = none (use GPIO CS) */
/* End-of-transfer reset (bcm2835_spi_reset_hw): TA cleared, write DONE (errata:
 * "transmission breaks unless DONE written each xfer"), flush both FIFOs. */
#define BCM_SPI_CS_RESET     (BCM_SPI_CS_DONE | BCM_SPI_CS_CLEAR_RX | BCM_SPI_CS_CLEAR_TX)

typedef struct _BCM_SPI_HW {
    PUCHAR  Base;
    SIZE_T  Length;
    UINT8   Mode;     /* SPI mode 0..3 */
} BCM_SPI_HW, *PBCM_SPI_HW;

FORCEINLINE UINT32 BcmSpiRead(_In_ PBCM_SPI_HW Hw, _In_ ULONG Reg)
{ return READ_REGISTER_ULONG((volatile ULONG *)(Hw->Base + Reg)); }
FORCEINLINE VOID BcmSpiWrite(_In_ PBCM_SPI_HW Hw, _In_ ULONG Reg, _In_ UINT32 Val)
{ WRITE_REGISTER_ULONG((volatile ULONG *)(Hw->Base + Reg), Val); }

VOID     BcmSpiHwInit(_Out_ PBCM_SPI_HW Hw, _In_ PUCHAR Base, _In_ SIZE_T Length);
VOID     BcmSpiHwSetClock(_Inout_ PBCM_SPI_HW Hw, _In_ UINT16 Div);
UINT16   BcmSpiHwClkDiv(_In_ UINT32 SclkHz);   /* CDIV from target SCLK @ core=750MHz */
VOID     BcmSpiHwSetMode(_Inout_ PBCM_SPI_HW Hw, _In_ UINT8 Mode);
NTSTATUS BcmSpiHwTransferPolled(_Inout_ PBCM_SPI_HW Hw,
                                _In_reads_opt_(Len) const UCHAR *Tx,
                                _Out_writes_opt_(Len) UCHAR *Rx, _In_ ULONG Len);
