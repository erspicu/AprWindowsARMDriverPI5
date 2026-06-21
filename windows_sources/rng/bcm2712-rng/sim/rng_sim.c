/*++ rng_sim.c - x64 simulation of the BCM2712 RNG200 HAL. Build: cl /DRNG_SIM /I.. rng_sim.c ..\rng_hw.c --*/
#include <stdio.h>
#include <string.h>
#include "../rng_hw.h"

unsigned char *g_rngBase;
static unsigned char g_regs[0x40];
static unsigned load(unsigned off)         { unsigned v=0; int i; for(i=0;i<4;i++) v|=((unsigned)g_regs[off+i])<<(8*i); return v; }
static void     store(unsigned off,unsigned v){ int i; for(i=0;i<4;i++) g_regs[off+i]=(unsigned char)((v>>(8*i))&0xFF); }
unsigned int RngSimRd(unsigned off)        { return load(off); }
void         RngSimWr(unsigned off, unsigned val){ store(off, val); }

static int g_pass, g_fail;
static void check(const char *what, int c){ if(c){g_pass++;printf("  [PASS] %s\n",what);}else{g_fail++;printf("  [FAIL] %s\n",what);} }

int main(void)
{
    memset(g_regs, 0, sizeof(g_regs));
    g_rngBase = g_regs;
    printf("== BCM2712 RNG200 HAL simulation ==\n");

    RngHwInit(g_regs);   /* CTRL starts 0 (not enabled) -> full bcm2711 init runs */
    check("INT_STATUS cleared (0xFFFFFFFF written)", load(RNG_INT_STATUS) == 0xFFFFFFFFu);
    check("bit-count threshold == 0x40000", load(RNG_TOTAL_BIT_COUNT_THRESHOLD) == 0x40000);
    check("FIFO threshold == 2<<8", load(RNG_FIFO_COUNT) == (2u << RNG_FIFO_THRESHOLD_SHIFT));
    check("CTRL == DIV_CTRL(0x3<<13)|RBGEN_MASK", load(RNG_CTRL) == ((0x3u<<RNG_CTRL_DIV_CTRL_SHIFT)|RNG_CTRL_RBGEN_MASK));
    check("CTRL RBGEN enabled", (load(RNG_CTRL) & RNG_CTRL_RBGEN_MASK) != 0);

    /* warm-up gate: read only after TOTAL_BIT_COUNT > 16 */
    store(RNG_TOTAL_BIT_COUNT, 8);
    check("WaitWarmup times out when count<=16", RngHwWaitWarmup(g_regs) == 0);
    store(RNG_TOTAL_BIT_COUNT, 32);
    check("WaitWarmup succeeds when count>16", RngHwWaitWarmup(g_regs) == 1);

    /* re-init when already enabled is a no-op (returns early) */
    store(RNG_TOTAL_BIT_COUNT_THRESHOLD, 0);
    RngHwInit(g_regs);
    check("re-init no-op when RBGEN already set", load(RNG_TOTAL_BIT_COUNT_THRESHOLD) == 0);

    store(RNG_FIFO_COUNT, 0x00000505);   /* 5 words available, threshold bits in high byte */
    check("FifoCount masks to 5", RngHwFifoCount(g_regs) == 5);

    store(RNG_FIFO_DATA, 0xDEADBEEF);
    check("ReadWord returns FIFO_DATA", RngHwReadWord(g_regs) == 0xDEADBEEFu);

    RngHwDisable(g_regs);
    check("Disable clears RBGEN_ENABLE", (load(RNG_CTRL) & RNG_CTRL_RBGEN_ENABLE) == 0);

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
