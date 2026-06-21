/*++
Module Name: bcmspi_sim.c
Abstract:    x64 simulation of the BCM2835/BCM2712 SPI HAL (bcm_spi_hw.c). Models
             the CS TA / TXD / RXD / DONE full-duplex handshake.
             Build: cl /DBCMSPI_SIM /I.. bcmspi_sim.c ..\bcm_spi_hw.c
--*/
#include <stdio.h>
#include "../bcm_spi_hw.h"

unsigned char *g_bspiBase;
static unsigned char g_regs[0x40];
static unsigned g_txLog[64]; static int g_txN;
static unsigned char g_rxq[64]; static int g_rxHead, g_rxTail;
static const unsigned char g_rxSrc[] = { 0x5A, 0xA5, 0x3C, 0xC3 };
static int g_rxSrcN;
static unsigned g_csStart;   /* CS value captured when TA was asserted */

static unsigned load(unsigned off)         { unsigned v=0; int i; for(i=0;i<4;i++) v|=((unsigned)g_regs[off+i])<<(8*i); return v; }
static void     store(unsigned off,unsigned v){ int i; for(i=0;i<4;i++) g_regs[off+i]=(unsigned char)((v>>(8*i))&0xFF); }

unsigned int BspiSimRd(unsigned off)
{
    if (off == BCM_SPI_CS) {
        unsigned s = (load(BCM_SPI_CS) & BCM_SPI_CS_TA);
        s |= BCM_SPI_CS_TXD;                                 /* FIFO always has room */
        if (g_rxHead < g_rxTail) s |= BCM_SPI_CS_RXD;        /* rx data pending */
        else if (g_txN > 0)      s |= BCM_SPI_CS_DONE;       /* all shifted + drained */
        return s;
    }
    if (off == BCM_SPI_FIFO) {
        return (g_rxHead < g_rxTail) ? g_rxq[g_rxHead++] : 0;
    }
    return load(off);
}

void BspiSimWr(unsigned off, unsigned val)
{
    if (off == BCM_SPI_CS) {
        store(off, val);
        if (val & BCM_SPI_CS_TA) {                           /* start: reset fifo state */
            g_csStart = val;
            g_txN = 0; g_rxHead = g_rxTail = 0; g_rxSrcN = 0;
        }
        return;
    }
    if (off == BCM_SPI_FIFO) {                               /* tx -> shift in an rx (full-duplex) */
        if (g_txN < 64) g_txLog[g_txN++] = val;
        g_rxq[g_rxTail++] = g_rxSrc[g_rxSrcN++ % (int)sizeof(g_rxSrc)];
        return;
    }
    store(off, val);
}

static int g_pass, g_fail;
static void check(const char *what, int cond)
{
    if (cond) { g_pass++; printf("  [PASS] %s\n", what); }
    else      { g_fail++; printf("  [FAIL] %s\n", what); }
}

int main(void)
{
    BCM_SPI_HW hw;
    NTSTATUS   st;
    unsigned char tx[2] = { 0xAA, 0xBB };
    unsigned char rx[2] = { 0, 0 };
    unsigned char rd[2] = { 0, 0 };

    memset(g_regs, 0, sizeof(g_regs));
    g_bspiBase = g_regs;
    printf("== BCM SPI transfer-engine simulation ==\n");

    BcmSpiHwInit(&hw, g_regs, sizeof(g_regs));
    BcmSpiHwSetClock(&hw, 0x100);
    check("CLK divider == 0x100", load(BCM_SPI_CLK) == 0x100);

    BcmSpiHwSetMode(&hw, 3);
    check("Mode == 3", hw.Mode == 3);

    /* full-duplex 2-byte transfer, mode 3 */
    st = BcmSpiHwTransferPolled(&hw, tx, rx, 2);
    check("TransferPolled succeeds", NT_SUCCESS(st));
    check("start CS had TA", (g_csStart & BCM_SPI_CS_TA) != 0);
    check("start CS had CPOL|CPHA (mode3)",
          (g_csStart & (BCM_SPI_CS_CPOL | BCM_SPI_CS_CPHA)) == (BCM_SPI_CS_CPOL | BCM_SPI_CS_CPHA));
    check("2 FIFO writes", g_txN == 2);
    check("FIFO bytes 0xAA 0xBB", (g_txLog[0]&0xFF)==0xAA && (g_txLog[1]&0xFF)==0xBB);
    check("rx[0] == 0x5A", rx[0] == 0x5A);
    check("rx[1] == 0xA5", rx[1] == 0xA5);
    check("end reset: TA cleared", (load(BCM_SPI_CS) & BCM_SPI_CS_TA) == 0);
    check("end reset: DONE+CLEAR_RX+CLEAR_TX written (errata)", load(BCM_SPI_CS) == BCM_SPI_CS_RESET);

    /* read-only (Tx NULL -> 0xFF idle) */
    st = BcmSpiHwTransferPolled(&hw, NULL, rd, 2);
    check("read-only transfer succeeds", NT_SUCCESS(st));
    check("idle TX bytes are 0xFF", (g_txLog[0]&0xFF)==0xFF && (g_txLog[1]&0xFF)==0xFF);

    /* CDIV computation from target SCLK @ core=750 MHz (Pi5-measured). */
    check("ClkDiv(0) -> default 0x100",   BcmSpiHwClkDiv(0)         == 0x100);
    check("ClkDiv(3MHz) == 250",          BcmSpiHwClkDiv(3000000)  == 250);
    check("ClkDiv(10MHz) == 76 (75->even)", BcmSpiHwClkDiv(10000000) == 76);
    check("ClkDiv(1MHz) == 750",          BcmSpiHwClkDiv(1000000)  == 750);
    check("ClkDiv(5kHz) == 0 (>=65536)",  BcmSpiHwClkDiv(5000)     == 0);

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
