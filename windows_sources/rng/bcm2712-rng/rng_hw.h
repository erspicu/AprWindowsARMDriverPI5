/*++
Module Name: rng_hw.h
Abstract:    BCM2712 iProc RNG200 HAL. Register map + enable/read, ported from
             Linux drivers/char/hw_random/iproc-rng200.c. Pure logic, sim-able.
--*/
#pragma once

#define RNG_CTRL              0x00
#define RNG_CTRL_RBGEN_MASK   0x00001FFF
#define RNG_CTRL_RBGEN_ENABLE 0x00000001
#define RNG_CTRL_DIV_CTRL_SHIFT 13          /* sample-rate divider (bcm2711) */
#define RNG_SOFT_RESET        0x04
#define RNG_TOTAL_BIT_COUNT   0x0C          /* warm-up progress counter */
#define RNG_TOTAL_BIT_COUNT_THRESHOLD 0x10  /* drop early less-random bits */
#define RNG_INT_STATUS        0x18
#define RNG_FIFO_DATA         0x20
#define RNG_FIFO_COUNT        0x24
#define RNG_FIFO_COUNT_MASK   0x000000FF
#define RNG_FIFO_THRESHOLD_SHIFT 8

void     RngHwInit(void *Base);       /* bcm2711 init: thresholds + sample rate + enable */
void     RngHwDisable(void *Base);    /* clear RBGEN enable */
int      RngHwWaitWarmup(void *Base); /* poll TOTAL_BIT_COUNT > 16; 1 ok, 0 timeout */
unsigned RngHwFifoCount(void *Base);  /* available 32-bit words */
unsigned RngHwReadWord(void *Base);   /* pop one random word */
