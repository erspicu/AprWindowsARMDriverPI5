/*++
Module Name: clk_hw.h
Abstract:    RP1 clock/PLL HAL. PLL configure + lock + clock gate, ported from
             Linux drivers/clk/clk-rp1.c. Pure logic, sim-able.
--*/
#pragma once

/* PLL bases */
#define PLL_SYS_OFFSET    0x08000
#define PLL_AUDIO_OFFSET  0x0c000
#define PLL_VIDEO_OFFSET  0x10000

/* per-PLL register sub-offsets */
#define PLL_CS            0x00
#define PLL_PWR           0x04
#define PLL_FBDIV_INT     0x08
#define PLL_FBDIV_FRAC    0x0c
#define PLL_PRIM          0x10
#define PLL_SEC           0x14

/* CS / PWR / PRIM fields */
#define PLL_CS_LOCK       (1u << 31)
#define PLL_CS_REFDIV_MASK  0x3Fu        /* CS[5:0] reference divider */
#define PLL_CS_REFDIV_UNITY 0x1u         /* rp1_pll_core_on sets REFDIV=1 */
#define PLL_PWR_PD        (1u << 0)
#define PLL_PWR_DSMPD     (1u << 2)
#define PLL_PWR_POSTDIVPD (1u << 3)
#define PLL_PWR_VCOPD     (1u << 5)
#define PLL_PWR_MASK      0x3Fu          /* GENMASK(5,0) */
#define PLL_PRIM_DIV1_SHIFT 16           /* GENMASK(18,16) */
#define PLL_PRIM_DIV2_SHIFT 12           /* GENMASK(14,12) */
#define PLL_PRIM_DIV1_MASK  (0x7u << 16)
#define PLL_PRIM_DIV2_MASK  (0x7u << 12)

/* generic clock gate */
#define CLK_CTRL_ENABLE   (1u << 11)

int  ClkHwPllConfig(void *Base, unsigned PllBase, unsigned FbDivInt,
                    unsigned FbDivFrac, unsigned Div1, unsigned Div2);   /* 0 ok, -1 no lock */
int  ClkHwPllIsLocked(void *Base, unsigned PllBase);
void ClkHwEnable(void *Base, unsigned CtrlOffset);
void ClkHwDisable(void *Base, unsigned CtrlOffset);
int  ClkHwIsEnabled(void *Base, unsigned CtrlOffset);
