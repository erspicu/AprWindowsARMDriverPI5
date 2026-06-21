/*++
Module Name: gpio_sim.c
Abstract:    x64 simulation of the RP1 GPIO interrupt HAL (rp1_gpio_hw.c). Mocks
             the GPIO io block (per-pin CTRL + per-bank PCIe INTE/INTS) and
             verifies the interrupt enable/mask/unmask/query/ack register logic,
             including pin->bank crossing.
             Build: cl /DGPIO_SIM /I.. gpio_sim.c ..\rp1_gpio_hw.c
--*/
#include <stdio.h>
#include <string.h>
#include "../rp1_gpio_hw.h"

unsigned char *g_gpioBase;
/* 0..0x9000 = GPIO io block (3 banks); PADS block placed at +0x10000 so pad
 * accesses (PadsBase + bank*0x4000 + ...) land in a distinct region. */
static unsigned char g_regs[0x1A000];

static unsigned load(unsigned off)         { unsigned v=0; int i; for(i=0;i<4;i++) v|=((unsigned)g_regs[off+i])<<(8*i); return v; }
static void     store(unsigned off,unsigned v){ int i; for(i=0;i<4;i++) g_regs[off+i]=(unsigned char)((v>>(8*i))&0xFF); }

unsigned int GpioSimRd(unsigned off)        { return load(off); }
void         GpioSimWr(unsigned off, unsigned val){ store(off, val); }

static int g_pass, g_fail;
static void check(const char *what, int cond)
{
    if (cond) { g_pass++; printf("  [PASS] %s\n", what); }
    else      { g_fail++; printf("  [FAIL] %s\n", what); }
}

int main(void)
{
    RP1_GPIO_HW hw;

    memset(g_regs, 0, sizeof(g_regs));
    memset(&hw, 0, sizeof(hw));
    hw.GpioBase = g_regs;
    hw.PadsBase = g_regs + 0x10000;   /* distinct PADS region */
    g_gpioBase  = g_regs;
    printf("== RP1 GPIO interrupt HAL simulation ==\n");

    /* function-select + bias: pin 7 (bank0, off7). CTRL @ 7*8+4 = 0x3C;
     * pad @ 0x10000 + 0x04 + 7*4 = 0x10020. */
    store(0x3C, 0x0000f000);   /* seed stale OUTOVER/OEOVER overrides */
    Rp1GpioSelectGpioFunction(&hw, 7);
    check("pin7 FUNCSEL == GPIO(5)", (load(0x3C) & RP1_GPIO_CTRL_FUNCSEL_MASK) == RP1_FSEL_GPIO);
    check("pin7 OUTOVER/OEOVER cleared to PERI", (load(0x3C) & (RP1_GPIO_CTRL_OUTOVER_MASK|RP1_GPIO_CTRL_OEOVER_MASK)) == 0);
    check("pin7 pad IN_ENABLE set", (load(0x10020) & RP1_PAD_IN_ENABLE) != 0);
    check("pin7 pad OUT_DISABLE cleared", (load(0x10020) & RP1_PAD_OUT_DISABLE) == 0);
    Rp1GpioSetPull(&hw, 7, RP1_PUD_UP);
    check("pin7 pad PULL == UP", ((load(0x10020) & RP1_PAD_PULL_MASK) >> RP1_PAD_PULL_LSB) == RP1_PUD_UP);
    Rp1GpioSetPull(&hw, 7, RP1_PUD_DOWN);
    check("pin7 pad PULL re-set == DOWN", ((load(0x10020) & RP1_PAD_PULL_MASK) >> RP1_PAD_PULL_LSB) == RP1_PUD_DOWN);

    /* pin 5 (bank0, off5): rising-edge interrupt */
    Rp1GpioEnableIrq(&hw, 5, TRUE, FALSE, FALSE, FALSE);
    check("pin5 CTRL has IRQEN_RISING", (load(5*8 + RP1_GPIO_CTRL) & RP1_GPIO_CTRL_IRQEN_RISING) != 0);
    check("pin5 not falling", (load(5*8 + RP1_GPIO_CTRL) & RP1_GPIO_CTRL_IRQEN_FALLING) == 0);
    check("bank0 INTE bit5 set", (load(RP1_GPIO_PCIE_INTE) & (1u << 5)) != 0);

    /* pin 10 (bank0, off10): falling-edge */
    Rp1GpioEnableIrq(&hw, 10, FALSE, TRUE, FALSE, FALSE);
    check("pin10 CTRL has IRQEN_FALLING", (load(10*8 + RP1_GPIO_CTRL) & RP1_GPIO_CTRL_IRQEN_FALLING) != 0);
    check("bank0 INTE bit10 set", (load(RP1_GPIO_PCIE_INTE) & (1u << 10)) != 0);

    /* mask / unmask pin5 (per-bank INTE) */
    Rp1GpioMaskIrq(&hw, 5);
    check("pin5 masked (INTE bit5 clear)", (load(RP1_GPIO_PCIE_INTE) & (1u << 5)) == 0);
    check("pin10 still enabled", (load(RP1_GPIO_PCIE_INTE) & (1u << 10)) != 0);
    Rp1GpioUnmaskIrq(&hw, 5);
    check("pin5 unmasked (INTE bit5 set)", (load(RP1_GPIO_PCIE_INTE) & (1u << 5)) != 0);

    /* query active: preset bank0 INTS, read it */
    store(RP1_GPIO_PCIE_INTS, (1u << 5) | (1u << 10));
    check("QueryActive bank0 == pins 5,10", Rp1GpioQueryActiveIrq(&hw, 0) == ((1u << 5) | (1u << 10)));

    /* ack pin5 -> CTRL IRQRESET */
    Rp1GpioAckIrq(&hw, 5);
    check("pin5 CTRL has IRQRESET", (load(5*8 + RP1_GPIO_CTRL) & RP1_GPIO_CTRL_IRQRESET) != 0);

    /* bank crossing: pin 30 -> bank1 (off2), high-level */
    Rp1GpioEnableIrq(&hw, 30, FALSE, FALSE, TRUE, FALSE);
    check("pin30 in bank1: CTRL @0x4014 has IRQEN_HIGH",
          (load(0x4000 + 2*8 + RP1_GPIO_CTRL) & RP1_GPIO_CTRL_IRQEN_HIGH) != 0);
    check("bank1 INTE (@0x411c) bit2 set",
          (load(0x4000 + RP1_GPIO_PCIE_INTE) & (1u << 2)) != 0);

    /* bank2: pin 40 -> bank2 (off6) */
    Rp1GpioEnableIrq(&hw, 40, TRUE, TRUE, FALSE, FALSE);
    check("pin40 in bank2 INTE (@0x811c) bit6 set",
          (load(0x8000 + RP1_GPIO_PCIE_INTE) & (1u << 6)) != 0);

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
