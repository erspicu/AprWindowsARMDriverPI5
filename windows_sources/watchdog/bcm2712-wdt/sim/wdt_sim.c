/*++ wdt_sim.c - x64 simulation of the BCM2712 PM watchdog HAL. Build: cl /DWDT_SIM /I.. wdt_sim.c ..\wdt_hw.c --*/
#include <stdio.h>
#include <string.h>
#include "../wdt_hw.h"

unsigned char *g_wdtBase;
static unsigned char g_regs[0x40];
static unsigned load(unsigned off)         { unsigned v=0; int i; for(i=0;i<4;i++) v|=((unsigned)g_regs[off+i])<<(8*i); return v; }
static void     store(unsigned off,unsigned v){ int i; for(i=0;i<4;i++) g_regs[off+i]=(unsigned char)((v>>(8*i))&0xFF); }
unsigned int WdtSimRd(unsigned off)        { return load(off); }
void         WdtSimWr(unsigned off, unsigned val){ store(off, val); }

static int g_pass, g_fail;
static void check(const char *what, int c){ if(c){g_pass++;printf("  [PASS] %s\n",what);}else{g_fail++;printf("  [FAIL] %s\n",what);} }

int main(void)
{
    memset(g_regs, 0, sizeof(g_regs));
    g_wdtBase = g_regs;
    printf("== BCM2712 PM watchdog HAL simulation ==\n");

    WdtHwStart(g_regs, 10);   /* 10 second timeout */
    check("WDOG carries PM_PASSWORD", (load(PM_WDOG) & 0xFF000000u) == PM_PASSWORD);
    check("WDOG ticks == 10s (10<<16)", (load(PM_WDOG) & PM_WDOG_TIME_SET) == (10u << 16));
    check("RSTC carries PM_PASSWORD", (load(PM_RSTC) & 0xFF000000u) == PM_PASSWORD);
    check("RSTC has WRCFG_FULL_RESET", (load(PM_RSTC) & PM_RSTC_WRCFG_FULL_RESET) != 0);
    check("IsRunning true", WdtHwIsRunning(g_regs) == 1);
    check("TimeLeft == 10s", WdtHwTimeLeftSecs(g_regs) == 10);

    WdtHwPing(g_regs, 12);    /* re-arm to 12s (max ~15s: 0xfffff>>16) */
    check("Ping re-arms WDOG to 12s", (load(PM_WDOG) & PM_WDOG_TIME_SET) == (12u << 16));
    check("TimeLeft == 12s after ping", WdtHwTimeLeftSecs(g_regs) == 12);

    /* clamp: timeout beyond the 15s max clamps to PM_WDOG_TIME_SET */
    WdtHwStart(g_regs, 0x100);
    check("huge timeout clamps to TIME_SET", (load(PM_WDOG) & PM_WDOG_TIME_SET) == PM_WDOG_TIME_SET);

    WdtHwStop(g_regs);
    check("Stop writes RSTC RESET", (load(PM_RSTC) & 0x00FFFFFFu) == PM_RSTC_RESET);
    check("IsRunning false after stop", WdtHwIsRunning(g_regs) == 0);

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
