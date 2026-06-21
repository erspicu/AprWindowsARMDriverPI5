/*++ rng_hw.c - BCM2712 iProc RNG200 HAL (ported from iproc-rng200.c). --*/
#include "rng_regio.h"
#include "rng_hw.h"

/* bcm2711_rng200_init: the generic iproc path only flips RBGEN; the BCM2711/2712
 * variant must also set the bit-count threshold (drop early low-entropy bits),
 * the FIFO threshold, and enable with the sample-rate divider. */
void RngHwInit(void *Base)
{
    WR32(Base, RNG_INT_STATUS, 0xFFFFFFFFu);   /* clear all interrupt status */
    if (RD32(Base, RNG_CTRL) & RNG_CTRL_RBGEN_MASK) {
        return;                                 /* already enabled */
    }
    WR32(Base, RNG_TOTAL_BIT_COUNT_THRESHOLD, 0x40000);
    WR32(Base, RNG_FIFO_COUNT, 2u << RNG_FIFO_THRESHOLD_SHIFT);
    WR32(Base, RNG_CTRL, (0x3u << RNG_CTRL_DIV_CTRL_SHIFT) | RNG_CTRL_RBGEN_MASK);
}

/* bcm2711_rng200_read waits for warm-up (TOTAL_BIT_COUNT > 16) before reading. */
int RngHwWaitWarmup(void *Base)
{
    unsigned i;
    for (i = 0; i < 100000u; i++) {
        if (RD32(Base, RNG_TOTAL_BIT_COUNT) > 16) {
            return 1;
        }
    }
    return 0;
}

void RngHwDisable(void *Base)
{
    unsigned v = RD32(Base, RNG_CTRL);
    v &= ~RNG_CTRL_RBGEN_MASK;                  /* clears RBGEN enable */
    WR32(Base, RNG_CTRL, v);
}

unsigned RngHwFifoCount(void *Base)
{
    return RD32(Base, RNG_FIFO_COUNT) & RNG_FIFO_COUNT_MASK;
}

unsigned RngHwReadWord(void *Base)
{
    return RD32(Base, RNG_FIFO_DATA);
}
