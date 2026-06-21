/*++
Module Name: gem_sim.c
Abstract:    x64 simulation of the Cadence GEM HAL (gem_hw.c). Verifies MAC/
             config/ring register programming + the DMA descriptor builders +
             MDIO MAN word, without hardware.
             Build: cl /DGEM_SIM /I.. gem_sim.c ..\gem_hw.c
--*/
#include <stdio.h>
#include <string.h>
#include "../gem_hw.h"

unsigned char *g_gemBase;
static unsigned char g_regs[0x100];

static unsigned load(unsigned off)         { unsigned v=0; int i; for(i=0;i<4;i++) v|=((unsigned)g_regs[off+i])<<(8*i); return v; }
static void     store(unsigned off,unsigned v){ int i; for(i=0;i<4;i++) g_regs[off+i]=(unsigned char)((v>>(8*i))&0xFF); }

unsigned int GemSimRd(unsigned off)        { return load(off); }
void         GemSimWr(unsigned off, unsigned val){ store(off, val); }

static int g_pass, g_fail;
static void check(const char *what, int cond)
{
    if (cond) { g_pass++; printf("  [PASS] %s\n", what); }
    else      { g_fail++; printf("  [FAIL] %s\n", what); }
}

int main(void)
{
    const unsigned char mac[6] = { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 };
    GEM_DESC tx, rx;
    unsigned int man;

    memset(g_regs, 0, sizeof(g_regs));
    g_gemBase = g_regs;
    printf("== Cadence GEM (Ethernet) engine simulation ==\n");

    GemHwReset(g_gemBase);
    check("Reset: NCR == 0", load(GEM_NCR) == 0);
    check("Reset: IDR masked all", load(GEM_IDR) == 0xFFFFFFFFu);

    GemSetMacAddr(g_gemBase, mac);
    check("SA1B == 33221102", load(GEM_SA1B) == 0x33221102u);
    check("SA1T == 5544",     load(GEM_SA1T) == 0x00005544u);

    GemConfigure(g_gemBase, 1, 1);
    check("NCFGR == CLK_DIV96|DRFCS|SPD|FD",
          load(GEM_NCFGR) == (GEM_NCFGR_CLK_DIV96 | GEM_NCFGR_DRFCS | GEM_NCFGR_SPD | GEM_NCFGR_FD));
    check("NCFGR MDC divider field == DIV96 (5)", ((load(GEM_NCFGR) >> 18) & 0x7) == 5);
    check("NCFGR DRFCS set (discard RX FCS)", (load(GEM_NCFGR) & GEM_NCFGR_DRFCS) != 0);

    GemSetRings(g_gemBase, 0x10000000u, 0x20000000u);
    check("RBQP == rx ring", load(GEM_RBQP) == 0x10000000u);
    check("TBQP == tx ring", load(GEM_TBQP) == 0x20000000u);

    GemEnable(g_gemBase);
    check("NCR == RXEN|TXEN|MPE", load(GEM_NCR) == (GEM_NCR_RXEN | GEM_NCR_TXEN | GEM_NCR_MPE));

    /* TX descriptor (last, wrap) */
    GemTxBuildDesc(&tx, 0x80000000u, 100, 1, 1);
    check("TX desc addr", tx.addr == 0x80000000u);
    check("TX desc len == 100", (tx.ctrl & GEM_TX_FRMLEN_MASK) == 100);
    check("TX desc LAST|WRAP set", (tx.ctrl & (GEM_TX_LAST | GEM_TX_WRAP)) == (GEM_TX_LAST | GEM_TX_WRAP));
    check("TX desc USED clear (HW owns)", (tx.ctrl & GEM_TX_USED) == 0);

    /* RX descriptor init then a HW-filled state */
    GemRxInitDesc(&rx, 0x90000004u, 1);
    check("RX init: addr aligned + WRAP", rx.addr == ((0x90000004u & GEM_RX_ADDR_MASK) | GEM_RX_WRAP));
    check("RX init: USED clear (HW owns)", GemRxDescIsUsed(&rx) == 0);
    rx.addr |= GEM_RX_USED;                 /* HW filled it */
    rx.ctrl = 64 | GEM_RX_SOF | GEM_RX_EOF;
    check("RX filled: IsUsed == 1", GemRxDescIsUsed(&rx) == 1);
    check("RX filled: length == 64", GemRxDescLength(&rx) == 64);

    /* MDIO read of PHY 1 reg 2 */
    man = GemMdioBuild(1, 2, 0, 0);
    check("MDIO SOF == 01", ((man >> 30) & 3) == 1);
    check("MDIO op == READ", ((man >> 28) & 3) == GEM_MAN_READ);
    check("MDIO phy == 1", ((man >> 23) & 0x1F) == 1);
    check("MDIO reg == 2", ((man >> 18) & 0x1F) == 2);
    /* MDIO write carries data */
    man = GemMdioBuild(3, 4, 1, 0xBEEF);
    check("MDIO write op", ((man >> 28) & 3) == GEM_MAN_WRITE);
    check("MDIO write data == 0xBEEF", (man & 0xFFFF) == 0xBEEF);

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
