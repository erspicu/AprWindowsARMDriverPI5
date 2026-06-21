/*++ dma_sim.c - x64 simulation of the BCM2712 DMA HAL. Build: cl /DDMA_SIM /I.. dma_sim.c ..\dma_hw.c --*/
#include <stdio.h>
#include <string.h>
#include "../dma_hw.h"

unsigned char *g_dmaBase;
static unsigned char g_regs[0x1000];   /* covers channels + global @0xfe0/0xff0 */
static unsigned load(unsigned off)         { unsigned v=0; int i; for(i=0;i<4;i++) v|=((unsigned)g_regs[off+i])<<(8*i); return v; }
static void     store(unsigned off,unsigned v){ int i; for(i=0;i<4;i++) g_regs[off+i]=(unsigned char)((v>>(8*i))&0xFF); }
unsigned int DmaSimRd(unsigned off)        { return load(off); }
void         DmaSimWr(unsigned off, unsigned val)
{
    store(off, val);
    /* model: RESET bit is self-clearing */
    if ((off % BCM2835_DMA_CHAN_SIZE) == BCM2835_DMA_CS && (val & BCM2835_DMA_RESET)) {
        store(off, val & ~BCM2835_DMA_RESET);
    }
}

static int g_pass, g_fail;
static void check(const char *what, int c){ if(c){g_pass++;printf("  [PASS] %s\n",what);}else{g_fail++;printf("  [FAIL] %s\n",what);} }

int main(void)
{
    memset(g_regs, 0, sizeof(g_regs));
    g_dmaBase = g_regs;
    printf("== BCM2712 DMA HAL simulation ==\n");

    DmaHwReset(g_regs, 0);
    check("ch0 RESET self-cleared", (load(DMA_CHAN_BASE(0)+BCM2835_DMA_CS) & BCM2835_DMA_RESET) == 0);

    DmaHwEnable(g_regs, 0);
    DmaHwEnable(g_regs, 3);
    check("global ENABLE bit0 set", (load(BCM2835_DMA_ENABLE) & (1u<<0)) != 0);
    check("global ENABLE bit3 set", (load(BCM2835_DMA_ENABLE) & (1u<<3)) != 0);

    DmaHwStart(g_regs, 0, 0x00010000);
    check("ch0 CONBLK_AD == 0x10000", load(DMA_CHAN_BASE(0)+BCM2835_DMA_ADDR) == 0x00010000u);
    check("ch0 CS has ACTIVE", (load(DMA_CHAN_BASE(0)+BCM2835_DMA_CS) & BCM2835_DMA_ACTIVE) != 0);
    check("IsActive ch0 true", DmaHwIsActive(g_regs, 0) == 1);

    /* channel 1 at +0x100 */
    DmaHwStart(g_regs, 1, 0x00020000);
    check("ch1 CONBLK_AD @0x104 == 0x20000", load(DMA_CHAN_BASE(1)+BCM2835_DMA_ADDR) == 0x00020000u);
    check("ch1 independent of ch0", DmaHwIsActive(g_regs, 1) == 1 && DmaHwIsActive(g_regs, 0) == 1);

    /* completion + ack */
    store(DMA_CHAN_BASE(0)+BCM2835_DMA_CS, BCM2835_DMA_END | BCM2835_DMA_INT);
    check("IsDone ch0 true", DmaHwIsDone(g_regs, 0) == 1);
    DmaHwAckInt(g_regs, 0);
    check("AckInt wrote END|INT (W1C)", (load(DMA_CHAN_BASE(0)+BCM2835_DMA_CS) & (BCM2835_DMA_END|BCM2835_DMA_INT)) == (BCM2835_DMA_END|BCM2835_DMA_INT));

    /* ===== BCM2712 DMA40 (40-bit) path ===== */
    memset(g_regs, 0, sizeof(g_regs));

    /* reset on an idle channel (CB == 0) is a no-op */
    DmaHw40Reset(g_regs, 2);
    check("DMA40 reset idle (CB==0) no DEBUG_RESET", (load(DMA_CHAN_BASE(2)+BCM2711_DMA40_DEBUG) & BCM2711_DMA40_DEBUG_RESET) == 0);

    /* start: 40-bit CB = paddr>>5, CS = ACTIVE|PROT|flags */
    DmaHw40Start(g_regs, 2, DMA40_TO_CBADDR(0x1f00188000ULL), 0);
    check("DMA40 CB == paddr>>5", load(DMA_CHAN_BASE(2)+BCM2711_DMA40_CB) == DMA40_TO_CBADDR(0x1f00188000ULL));
    check("DMA40 CS has ACTIVE|PROT", (load(DMA_CHAN_BASE(2)+BCM2711_DMA40_CS) & (BCM2711_DMA40_ACTIVE|BCM2711_DMA40_PROT)) == (BCM2711_DMA40_ACTIVE|BCM2711_DMA40_PROT));
    check("DMA40 IsActive ch2 true", DmaHw40IsActive(g_regs, 2) == 1);

    /* reset now (CB != 0): pause -> PROT default -> DEBUG_RESET pulse */
    DmaHw40Reset(g_regs, 2);
    check("DMA40 reset sets DEBUG_RESET", (load(DMA_CHAN_BASE(2)+BCM2711_DMA40_DEBUG) & BCM2711_DMA40_DEBUG_RESET) != 0);
    check("DMA40 reset clears ACTIVE (CS=PROT)", (load(DMA_CHAN_BASE(2)+BCM2711_DMA40_CS) & BCM2711_DMA40_ACTIVE) == 0);

    /* completion + ack keeps the channel active (cyclic-safe) */
    DmaHw40Start(g_regs, 2, DMA40_TO_CBADDR(0x1f00188000ULL), 0);
    store(DMA_CHAN_BASE(2)+BCM2711_DMA40_CS, BCM2711_DMA40_END);
    check("DMA40 IsDone ch2 true", DmaHw40IsDone(g_regs, 2) == 1);
    DmaHw40AckInt(g_regs, 2, 0);
    check("DMA40 Ack writes INT", (load(DMA_CHAN_BASE(2)+BCM2711_DMA40_CS) & BCM2711_DMA40_INT) != 0);
    check("DMA40 Ack keeps ACTIVE (cyclic)", (load(DMA_CHAN_BASE(2)+BCM2711_DMA40_CS) & BCM2711_DMA40_ACTIVE) != 0);

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
