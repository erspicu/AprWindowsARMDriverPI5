/*++ clk_hw.c - RP1 clock/PLL HAL (ported from clk-rp1.c). --*/
#include "clk_regio.h"
#include "clk_hw.h"

#define CLK_POLL_MAX 100000

int ClkHwPllConfig(void *Base, unsigned PllBase, unsigned FbDivInt,
                   unsigned FbDivFrac, unsigned Div1, unsigned Div2)
{
    unsigned i, cs, prim;
    WR32(Base, PllBase + PLL_PWR, PLL_PWR_MASK);              /* power down */
    WR32(Base, PllBase + PLL_FBDIV_INT, FbDivInt);
    WR32(Base, PllBase + PLL_FBDIV_FRAC, FbDivFrac);
    /* Set reference divider to 1 (rp1_pll_core_on / _set_rate). Without this a
     * stale REFDIV mis-divides the VCO reference -> wrong freq / no lock. RMW so
     * the read-only LOCK and other CS bits are preserved. */
    cs = RD32(Base, PllBase + PLL_CS);
    cs = (cs & ~PLL_CS_REFDIV_MASK) | PLL_CS_REFDIV_UNITY;
    WR32(Base, PllBase + PLL_CS, cs);
    WR32(Base, PllBase + PLL_PWR, FbDivFrac ? 0u : PLL_PWR_DSMPD);  /* power up */
    for (i = 0; i < CLK_POLL_MAX; i++) {
        if (RD32(Base, PllBase + PLL_CS) & PLL_CS_LOCK) {
            break;
        }
    }
    if (i == CLK_POLL_MAX) {
        return -1;
    }
    /* Program the post-dividers with read-modify-write: only touch DIV1/DIV2
     * (rp1_pll_set_rate), preserving other PLL_PRIM bits (prev code clobbered them). */
    prim = RD32(Base, PllBase + PLL_PRIM);
    prim &= ~(PLL_PRIM_DIV1_MASK | PLL_PRIM_DIV2_MASK);
    prim |= ((Div1 & 0x7u) << PLL_PRIM_DIV1_SHIFT) | ((Div2 & 0x7u) << PLL_PRIM_DIV2_SHIFT);
    WR32(Base, PllBase + PLL_PRIM, prim);
    return 0;
}

int ClkHwPllIsLocked(void *Base, unsigned PllBase)
{
    return (RD32(Base, PllBase + PLL_CS) & PLL_CS_LOCK) ? 1 : 0;
}

void ClkHwEnable(void *Base, unsigned CtrlOffset)
{
    WR32(Base, CtrlOffset, RD32(Base, CtrlOffset) | CLK_CTRL_ENABLE);
}

void ClkHwDisable(void *Base, unsigned CtrlOffset)
{
    WR32(Base, CtrlOffset, RD32(Base, CtrlOffset) & ~CLK_CTRL_ENABLE);
}

int ClkHwIsEnabled(void *Base, unsigned CtrlOffset)
{
    return (RD32(Base, CtrlOffset) & CLK_CTRL_ENABLE) ? 1 : 0;
}
