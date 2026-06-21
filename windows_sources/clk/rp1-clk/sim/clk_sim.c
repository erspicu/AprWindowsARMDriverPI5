/*++ clk_sim.c - x64 simulation of the RP1 clock/PLL HAL. Build: cl /DCLK_SIM /I.. clk_sim.c ..\clk_hw.c --*/
#include <stdio.h>
#include <string.h>
#include "../clk_hw.h"

unsigned char *g_clkBase;
static unsigned char g_regs[0x11000];   /* covers PLL_VIDEO @0x10000 */
static unsigned load(unsigned off)         { unsigned v=0; int i; for(i=0;i<4;i++) v|=((unsigned)g_regs[off+i])<<(8*i); return v; }
static void     store(unsigned off,unsigned v){ int i; for(i=0;i<4;i++) g_regs[off+i]=(unsigned char)((v>>(8*i))&0xFF); }
unsigned int ClkSimRd(unsigned off)        { return load(off); }
void         ClkSimWr(unsigned off, unsigned val)
{
    store(off, val);
    /* model: powering a PLL up (PWR write != full power-down mask) latches LOCK */
    if ((off & 0x1Fu) == PLL_PWR && val != PLL_PWR_MASK) {
        unsigned cs = off & ~0x1Fu;     /* CS is at the PLL base */
        store(cs, load(cs) | PLL_CS_LOCK);
    }
}

static int g_pass, g_fail;
static void check(const char *what, int c){ if(c){g_pass++;printf("  [PASS] %s\n",what);}else{g_fail++;printf("  [FAIL] %s\n",what);} }

int main(void)
{
    int rc;
    memset(g_regs, 0, sizeof(g_regs));
    g_clkBase = g_regs;
    printf("== RP1 clock/PLL HAL simulation ==\n");

    /* configure PLL_SYS: fbdiv 100 (integer), post-div 2/1 */
    rc = ClkHwPllConfig(g_regs, PLL_SYS_OFFSET, 100, 0, 2, 1);
    check("PllConfig returns 0 (locked)", rc == 0);
    check("FBDIV_INT == 100", load(PLL_SYS_OFFSET + PLL_FBDIV_INT) == 100);
    check("PWR power-up == DSMPD (no frac)", load(PLL_SYS_OFFSET + PLL_PWR) == PLL_PWR_DSMPD);
    check("PRIM div1=2 div2=1", load(PLL_SYS_OFFSET + PLL_PRIM) == ((2u<<16)|(1u<<12)));
    check("CS REFDIV field == 1 (unity)", (load(PLL_SYS_OFFSET + PLL_CS) & PLL_CS_REFDIV_MASK) == PLL_CS_REFDIV_UNITY);
    check("PllIsLocked true", ClkHwPllIsLocked(g_regs, PLL_SYS_OFFSET) == 1);

    /* fractional PLL keeps DSM enabled (PWR == 0) */
    rc = ClkHwPllConfig(g_regs, PLL_AUDIO_OFFSET, 80, 0x1234, 3, 2);
    check("fractional PllConfig returns 0", rc == 0);
    check("fractional PWR == 0 (DSM on)", load(PLL_AUDIO_OFFSET + PLL_PWR) == 0);
    check("FBDIV_FRAC == 0x1234", load(PLL_AUDIO_OFFSET + PLL_FBDIV_FRAC) == 0x1234);

    /* clock gate at an arbitrary CTRL offset */
    ClkHwEnable(g_regs, 0x100);
    check("clock CTRL has ENABLE (bit11)", (load(0x100) & CLK_CTRL_ENABLE) != 0);
    check("IsEnabled true", ClkHwIsEnabled(g_regs, 0x100) == 1);
    ClkHwDisable(g_regs, 0x100);
    check("IsEnabled false after disable", ClkHwIsEnabled(g_regs, 0x100) == 0);

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
