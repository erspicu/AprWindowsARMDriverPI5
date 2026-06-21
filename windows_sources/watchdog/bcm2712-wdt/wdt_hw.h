/*++
Module Name: wdt_hw.h
Abstract:    BCM2835/BCM2712 PM watchdog HAL. Ported from Linux
             drivers/watchdog/bcm2835_wdt.c. All writes carry PM_PASSWORD.
--*/
#pragma once

#define PM_RSTC                 0x1c
#define PM_RSTS                 0x20
#define PM_WDOG                 0x24
#define PM_PASSWORD             0x5a000000u
#define PM_WDOG_TIME_SET        0x000fffffu   /* 20-bit timeout in ticks */
#define PM_RSTC_WRCFG_CLR       0xffffffcfu
#define PM_RSTC_WRCFG_FULL_RESET 0x00000020u
#define PM_RSTC_RESET           0x00000102u
#define SECS_TO_WDOG_TICKS(x)   ((x) << 16)

void     WdtHwStart(void *Base, unsigned TimeoutSecs);  /* arm + enable full reset */
void     WdtHwPing(void *Base, unsigned TimeoutSecs);   /* re-arm the timer */
void     WdtHwStop(void *Base);
int      WdtHwIsRunning(void *Base);
unsigned WdtHwTimeLeftSecs(void *Base);
