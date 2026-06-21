/*++
Module Name: dwspi_sim.c
Abstract:    x64 simulation of the DesignWare SSI (SPI) HAL (dw_spi_hw.c). Mocks
             the DW SSI registers + full-duplex FIFO so the polled transfer can
             be exercised and asserted without hardware.
             Build: cl /DDWSPI_SIM /I.. dwspi_sim.c ..\dw_spi_hw.c
--*/
#include <stdio.h>
#include "../dw_spi_hw.h"

unsigned char *g_spiBase;
static unsigned char g_regs[0x100];
static unsigned g_txLog[64]; static int g_txN;          /* DR writes */
static unsigned char g_rxq[64]; static int g_rxHead, g_rxTail;
static const unsigned char g_rxSrc[] = { 0xA5, 0x5A, 0xC3, 0x3C };
static int g_rxSrcN;

static unsigned load(unsigned off)         { unsigned v=0; int i; for(i=0;i<4;i++) v|=((unsigned)g_regs[off+i])<<(8*i); return v; }
static void     store(unsigned off,unsigned v){ int i; for(i=0;i<4;i++) g_regs[off+i]=(unsigned char)((v>>(8*i))&0xFF); }

unsigned int SpiSimRd(unsigned off)
{
    switch (off) {
    case DW_SPI_VERSION: return 0x3430312Au;            /* "1.04" -> nonzero */
    case DW_SPI_SR: {
        unsigned s = DW_SPI_SR_TFNF | DW_SPI_SR_TFE;
        if (g_rxHead < g_rxTail) s |= DW_SPI_SR_RFNE;
        return s;
    }
    case DW_SPI_DR: return (g_rxHead < g_rxTail) ? g_rxq[g_rxHead++] : 0;
    default:        return load(off);
    }
}

void SpiSimWr(unsigned off, unsigned val)
{
    if (off == DW_SPI_DR) {                              /* full-duplex: each TX shifts in an RX */
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
    DW_SPI_HW hw;
    NTSTATUS  st;
    unsigned char tx[2] = { 0x9F, 0x00 };
    unsigned char rx[2] = { 0, 0 };
    unsigned char rd[2] = { 0, 0 };

    memset(g_regs, 0, sizeof(g_regs));
    g_spiBase = g_regs;
    printf("== DesignWare SSI (SPI) transfer-engine simulation ==\n");

    DwSpiHwInit(&hw, g_regs, sizeof(g_regs));
    /* seed leftover state to prove reset_chip clears it */
    store(DW_SPI_SER, 0x3);
    store(DW_SPI_IMR, 0xff);
    st = DwSpiHwProbe(&hw);
    check("Probe succeeds (version nonzero)", NT_SUCCESS(st));
    check("reset_chip cleared SER", load(DW_SPI_SER) == 0);
    check("reset_chip masked IMR", load(DW_SPI_IMR) == 0);
    check("reset_chip left chip enabled (SSIENR=1)", load(DW_SPI_SSIENR) == 1);
    /* mock CTRLR0 keeps written bits -> low 4 stick -> 4-bit DFS detected (offset 0) */
    check("DFS auto-detect -> 4-bit DFS (offset 0)", hw.DfsOffset == 0);

    /* mode 0, 8-bit, baud 16 */
    DwSpiHwConfigureMaster(&hw, 8, 0, 16);
    check("CTRLR0 = DFS7|TMOD_TR (mode0)", load(DW_SPI_CTRLR0) == 0x07u);
    check("BAUDR == 16", load(DW_SPI_BAUDR) == 16);
    check("SSIENR enabled", load(DW_SPI_SSIENR) == 1);

    /* mode 3 -> CPOL|CPHA set */
    DwSpiHwConfigureMaster(&hw, 8, 3, 8);
    check("CTRLR0 has SCPHA|SCPOL (mode3)",
          (load(DW_SPI_CTRLR0) & (DW_PSSI_CTRLR0_SCPHA | DW_PSSI_CTRLR0_SCPOL))
           == (DW_PSSI_CTRLR0_SCPHA | DW_PSSI_CTRLR0_SCPOL));
    check("BAUDR == 8", load(DW_SPI_BAUDR) == 8);

    DwSpiHwSetCs(&hw, 0, TRUE);
    check("SER == bit0 (CS0 asserted)", load(DW_SPI_SER) == 1);
    DwSpiHwSetCs(&hw, 0, FALSE);
    check("SER == 0 (CS deasserted)", load(DW_SPI_SER) == 0);

    /* full-duplex transfer of 2 bytes */
    g_txN = 0; g_rxHead = g_rxTail = 0; g_rxSrcN = 0;
    st = DwSpiHwTransferPolled(&hw, tx, rx, 2);
    check("TransferPolled succeeds", NT_SUCCESS(st));
    check("2 DR writes", g_txN == 2);
    check("DR write0 == 0x9F", (g_txLog[0] & 0xFF) == 0x9F);
    check("DR write1 == 0x00", (g_txLog[1] & 0xFF) == 0x00);
    check("rx[0] == 0xA5 (shifted in)", rx[0] == 0xA5);
    check("rx[1] == 0x5A", rx[1] == 0x5A);

    /* read-only transfer (Tx NULL -> sends 0xFF) */
    g_txN = 0; g_rxHead = g_rxTail = 0; g_rxSrcN = 0;
    st = DwSpiHwTransferPolled(&hw, NULL, rd, 2);
    check("read-only transfer succeeds", NT_SUCCESS(st));
    check("DR writes are 0xFF idle", (g_txLog[0] & 0xFF) == 0xFF && (g_txLog[1] & 0xFF) == 0xFF);

    /* SCKDV computation from target SCLK @ fssi=200 MHz (Pi5-measured). */
    check("BaudDiv(0) -> default 16",        DwSpiHwBaudDiv(0)         == 16);
    check("BaudDiv(12.5MHz) == 16",          DwSpiHwBaudDiv(12500000)  == 16);
    check("BaudDiv(8MHz) == 26 (25->even)",  DwSpiHwBaudDiv(8000000)   == 26);
    check("BaudDiv(100MHz) == 2 (max SCLK)", DwSpiHwBaudDiv(100000000) == 2);
    check("BaudDiv(1MHz) == 200",            DwSpiHwBaudDiv(1000000)   == 200);

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
