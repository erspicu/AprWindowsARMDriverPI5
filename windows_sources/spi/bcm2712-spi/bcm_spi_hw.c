/*++
Module Name: bcm_spi_hw.c
Abstract:    BCM2835/BCM2712 SPI HAL (master, polled full-duplex). Ported from
             spi-bcm2835.c (bcm2835_spi_transfer_one_poll).
--*/
#include "bcm_spi_hw.h"

#define BCM_SPI_POLL_SPINS   200000

VOID BcmSpiHwInit(_Out_ PBCM_SPI_HW Hw, _In_ PUCHAR Base, _In_ SIZE_T Length)
{
    RtlZeroMemory(Hw, sizeof(*Hw));
    Hw->Base = Base;
    Hw->Length = Length;
}

VOID BcmSpiHwSetClock(_Inout_ PBCM_SPI_HW Hw, _In_ UINT16 Div)
{
    /* Write CDIV verbatim. 0 is a VALID value (== 65536, slowest), as returned by
     * BcmSpiHwClkDiv for very low SCLK; callers pass an explicit default instead. */
    BcmSpiWrite(Hw, BCM_SPI_CLK, Div);
}

/* Compute CDIV for a target SCLK from the BCM2712 SPI core clock = vpu-clock =
 * 750 MHz (Pi5-measured: clk_summary vpu-clock -> 107d004000.spi). SCLK =
 * core / CDIV; CDIV must be even (HW ignores LSB); CDIV==0 means 65536 (slowest).
 * Ported from bcm2835_spi_transfer_one. See pi5-linux-hardware-facts note. */
UINT16 BcmSpiHwClkDiv(_In_ UINT32 SclkHz)
{
    UINT32 cdiv;
    if (SclkHz == 0) return 0x100;                       /* default ~2.93 MHz */
    cdiv = (BCM_SPI_CORE_HZ + SclkHz - 1) / SclkHz;      /* ceil */
    if (cdiv & 1) cdiv++;                                /* CDIV must be even */
    if (cdiv >= 65536) return 0;                         /* 0 == 65536 */
    return (UINT16)cdiv;
}

VOID BcmSpiHwSetMode(_Inout_ PBCM_SPI_HW Hw, _In_ UINT8 Mode)
{
    Hw->Mode = Mode & 0x3;
}

/* port of bcm2835_spi_transfer_one_poll(): assert TA, feed TX, drain RX. */
NTSTATUS BcmSpiHwTransferPolled(_Inout_ PBCM_SPI_HW Hw,
    _In_reads_opt_(Len) const UCHAR *Tx, _Out_writes_opt_(Len) UCHAR *Rx, _In_ ULONG Len)
{
    UINT32 cs;
    ULONG  txi = 0, rxi = 0, spins = 0;

    cs = BCM_SPI_CS_TA | BCM_SPI_CS_CLEAR_RX | BCM_SPI_CS_CLEAR_TX;
    cs |= BCM_SPI_CS_CS_MASK;   /* native CS=3 (none); avoid HW driving a CE pin */
    if (Hw->Mode & 0x1) cs |= BCM_SPI_CS_CPHA;
    if (Hw->Mode & 0x2) cs |= BCM_SPI_CS_CPOL;
    BcmSpiWrite(Hw, BCM_SPI_CS, cs);

    while (rxi < Len) {
        UINT32 s = BcmSpiRead(Hw, BCM_SPI_CS);

        if ((s & BCM_SPI_CS_TXD) && (txi < Len)) {
            BcmSpiWrite(Hw, BCM_SPI_FIFO, Tx ? Tx[txi] : 0xff);
            txi++; spins = 0;
            continue;
        }
        if (s & BCM_SPI_CS_RXD) {
            UCHAR b = (UCHAR)(BcmSpiRead(Hw, BCM_SPI_FIFO) & 0xff);
            if (Rx) Rx[rxi] = b;
            rxi++; spins = 0;
            continue;
        }
        if (++spins > BCM_SPI_POLL_SPINS) {
            BcmSpiWrite(Hw, BCM_SPI_CS, BCM_SPI_CS_RESET);  /* TA off, DONE, flush FIFOs */
            BcmSpiWrite(Hw, BCM_SPI_DLEN, 0);
            return STATUS_IO_TIMEOUT;
        }
        KeStallExecutionProcessor(1);
    }

    /* wait for DONE, then de-assert TA */
    for (spins = 0; spins < BCM_SPI_POLL_SPINS; spins++) {
        if (BcmSpiRead(Hw, BCM_SPI_CS) & BCM_SPI_CS_DONE) break;
        KeStallExecutionProcessor(1);
    }
    /* reset_hw: TA off + write DONE (errata) + flush FIFOs + DLEN=0 */
    BcmSpiWrite(Hw, BCM_SPI_CS, BCM_SPI_CS_RESET);
    BcmSpiWrite(Hw, BCM_SPI_DLEN, 0);
    return STATUS_SUCCESS;
}
