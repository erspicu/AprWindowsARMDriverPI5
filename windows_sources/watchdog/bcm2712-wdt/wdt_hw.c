/*++ wdt_hw.c - BCM2835/BCM2712 PM watchdog HAL (ported from bcm2835_wdt.c). --*/
#include "wdt_regio.h"
#include "wdt_hw.h"

static unsigned WdtTicks(unsigned TimeoutSecs)
{
    unsigned ticks = SECS_TO_WDOG_TICKS(TimeoutSecs);
    return (ticks > PM_WDOG_TIME_SET) ? PM_WDOG_TIME_SET : ticks;
}

void WdtHwStart(void *Base, unsigned TimeoutSecs)
{
    unsigned cur;
    WR32(Base, PM_WDOG, PM_PASSWORD | WdtTicks(TimeoutSecs));
    cur = RD32(Base, PM_RSTC);
    WR32(Base, PM_RSTC, PM_PASSWORD | (cur & PM_RSTC_WRCFG_CLR) | PM_RSTC_WRCFG_FULL_RESET);
}

void WdtHwPing(void *Base, unsigned TimeoutSecs)
{
    WR32(Base, PM_WDOG, PM_PASSWORD | WdtTicks(TimeoutSecs));
}

void WdtHwStop(void *Base)
{
    WR32(Base, PM_RSTC, PM_PASSWORD | PM_RSTC_RESET);
}

int WdtHwIsRunning(void *Base)
{
    return (RD32(Base, PM_RSTC) & PM_RSTC_WRCFG_FULL_RESET) ? 1 : 0;
}

unsigned WdtHwTimeLeftSecs(void *Base)
{
    return (RD32(Base, PM_WDOG) & PM_WDOG_TIME_SET) >> 16;
}
