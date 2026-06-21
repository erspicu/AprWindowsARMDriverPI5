/*++
Module Name: rp1_gpio_hw.c
Abstract:    RP1 GPIO HAL - pin read/write/direction via the RIO block, function
             select via per-pin CTRL. Ported from pinctrl-rp1.c.
--*/
#include "rp1_gpio_hw.h"

static const struct { ULONG First; ULONG Count; } g_Banks[3] = {
    { 0, 28 }, { 28, 6 }, { 34, 20 }
};

int Rp1GpioPinToBank(_In_ ULONG Pin, _Out_ ULONG *Offset)
{
    int b;
    for (b = 0; b < 3; b++) {
        if (Pin >= g_Banks[b].First && Pin < g_Banks[b].First + g_Banks[b].Count) {
            *Offset = Pin - g_Banks[b].First;
            return b;
        }
    }
    *Offset = 0;
    return -1;
}

static PUCHAR BankRio(_In_ PRP1_GPIO_HW Hw, _In_ int Bank)
{
    return Hw->RioBase + (ULONG)Bank * RP1_BANK_STRIDE;
}

static PUCHAR BankPad(_In_ PRP1_GPIO_HW Hw, _In_ int Bank, _In_ ULONG Off)
{
    return Hw->PadsBase + (ULONG)Bank * RP1_BANK_STRIDE + RP1_PAD_BANK_BASE + Off * 4;
}

BOOLEAN Rp1GpioReadPin(_In_ PRP1_GPIO_HW Hw, _In_ ULONG Pin)
{
    ULONG off;
    int b = Rp1GpioPinToBank(Pin, &off);
    ULONG v;
    if (b < 0 || Hw->RioBase == NULL) {
        return FALSE;
    }
    v = READ_REGISTER_ULONG((volatile ULONG *)(BankRio(Hw, b) + RP1_RIO_IN));
    return (v & (1u << off)) ? TRUE : FALSE;
}

VOID Rp1GpioWritePin(_In_ PRP1_GPIO_HW Hw, _In_ ULONG Pin, _In_ BOOLEAN Value)
{
    ULONG off;
    int b = Rp1GpioPinToBank(Pin, &off);
    ULONG alias;
    if (b < 0 || Hw->RioBase == NULL) {
        return;
    }
    alias = Value ? RP1_SET_OFFSET : RP1_CLR_OFFSET;   /* atomic set / clear */
    WRITE_REGISTER_ULONG((volatile ULONG *)(BankRio(Hw, b) + RP1_RIO_OUT + alias), (1u << off));
}

VOID Rp1GpioSetDirection(_In_ PRP1_GPIO_HW Hw, _In_ ULONG Pin, _In_ BOOLEAN Output)
{
    ULONG off;
    int b = Rp1GpioPinToBank(Pin, &off);
    ULONG alias;
    if (b < 0 || Hw->RioBase == NULL) {
        return;
    }
    alias = Output ? RP1_SET_OFFSET : RP1_CLR_OFFSET;   /* OE set=output, clr=input */
    WRITE_REGISTER_ULONG((volatile ULONG *)(BankRio(Hw, b) + RP1_RIO_OE + alias), (1u << off));
}

VOID Rp1GpioSelectGpioFunction(_In_ PRP1_GPIO_HW Hw, _In_ ULONG Pin)
{
    ULONG  off;
    int    b = Rp1GpioPinToBank(Pin, &off);
    PUCHAR ctrl;
    ULONG  v;
    if (b < 0 || Hw->GpioBase == NULL) {
        return;
    }
    ctrl = Hw->GpioBase + (ULONG)b * RP1_BANK_STRIDE + off * RP1_GPIO_PIN_STRIDE + RP1_GPIO_CTRL;
    v = READ_REGISTER_ULONG((volatile ULONG *)ctrl);
    /* rp1_set_fsel: select GPIO and force OUTOVER/OEOVER to PERI(0) so the RIO
     * actually drives the pad (a stale override would leave direction/output dead). */
    v = (v & ~(RP1_GPIO_CTRL_FUNCSEL_MASK | RP1_GPIO_CTRL_OUTOVER_MASK |
               RP1_GPIO_CTRL_OEOVER_MASK)) | RP1_FSEL_GPIO;
    WRITE_REGISTER_ULONG((volatile ULONG *)ctrl, v);

    /* enable both pad buffers (rp1_input_enable(1) + rp1_output_enable(1)); the
     * RIO OE then chooses the actual direction. Previously the PADS block (input
     * enable / output-disable) was never touched, so inputs could read nothing. */
    if (Hw->PadsBase != NULL) {
        PUCHAR pad = BankPad(Hw, b, off);
        ULONG  pv  = READ_REGISTER_ULONG((volatile ULONG *)pad);
        pv |=  RP1_PAD_IN_ENABLE;        /* input buffer on */
        pv &= ~RP1_PAD_OUT_DISABLE;      /* output buffer on */
        WRITE_REGISTER_ULONG((volatile ULONG *)pad, pv);
    }
}

/* Set the pad bias (pull-up/down/off). GpioClx passes this via the pin-connect
 * PullConfiguration; the prior HAL dropped it, leaving inputs (e.g. buttons)
 * floating. RP1_PUD_OFF/DOWN/UP -> PADS PULL field [3:2] (rp1_pull_config_set). */
VOID Rp1GpioSetPull(_In_ PRP1_GPIO_HW Hw, _In_ ULONG Pin, _In_ ULONG Pud)
{
    ULONG  off;
    int    b = Rp1GpioPinToBank(Pin, &off);
    PUCHAR pad;
    ULONG  v;
    if (b < 0 || Hw->PadsBase == NULL) {
        return;
    }
    pad = BankPad(Hw, b, off);
    v = READ_REGISTER_ULONG((volatile ULONG *)pad);
    v = (v & ~RP1_PAD_PULL_MASK) | ((Pud << RP1_PAD_PULL_LSB) & RP1_PAD_PULL_MASK);
    WRITE_REGISTER_ULONG((volatile ULONG *)pad, v);
}

/* ---------------- interrupt control ---------------- */

static PUCHAR PinCtrl(PRP1_GPIO_HW Hw, int Bank, ULONG Off)
{
    return Hw->GpioBase + (ULONG)Bank * RP1_BANK_STRIDE + Off * RP1_GPIO_PIN_STRIDE + RP1_GPIO_CTRL;
}
static PUCHAR BankReg(PRP1_GPIO_HW Hw, int Bank, ULONG Reg)
{
    return Hw->GpioBase + (ULONG)Bank * RP1_BANK_STRIDE + Reg;
}

VOID Rp1GpioEnableIrq(PRP1_GPIO_HW Hw, ULONG Pin,
                      BOOLEAN Rising, BOOLEAN Falling, BOOLEAN High, BOOLEAN Low)
{
    ULONG  off;
    int    b = Rp1GpioPinToBank(Pin, &off);
    PUCHAR ctrl, inte;
    ULONG  v;
    if (b < 0 || Hw->GpioBase == NULL) {
        return;
    }
    ctrl = PinCtrl(Hw, b, off);
    v = READ_REGISTER_ULONG((volatile ULONG *)ctrl) & ~RP1_GPIO_CTRL_IRQEN_MASK;
    if (Rising)  v |= RP1_GPIO_CTRL_IRQEN_RISING;
    if (Falling) v |= RP1_GPIO_CTRL_IRQEN_FALLING;
    if (High)    v |= RP1_GPIO_CTRL_IRQEN_HIGH;
    if (Low)     v |= RP1_GPIO_CTRL_IRQEN_LOW;
    WRITE_REGISTER_ULONG((volatile ULONG *)ctrl, v);
    /* route this pin's interrupt out via the per-bank PCIe INTE mask */
    inte = BankReg(Hw, b, RP1_GPIO_PCIE_INTE);
    WRITE_REGISTER_ULONG((volatile ULONG *)inte,
        READ_REGISTER_ULONG((volatile ULONG *)inte) | (1u << off));
}

VOID Rp1GpioMaskIrq(PRP1_GPIO_HW Hw, ULONG Pin)
{
    ULONG  off;
    int    b = Rp1GpioPinToBank(Pin, &off);
    PUCHAR inte;
    if (b < 0 || Hw->GpioBase == NULL) {
        return;
    }
    inte = BankReg(Hw, b, RP1_GPIO_PCIE_INTE);
    WRITE_REGISTER_ULONG((volatile ULONG *)inte,
        READ_REGISTER_ULONG((volatile ULONG *)inte) & ~(1u << off));
}

VOID Rp1GpioUnmaskIrq(PRP1_GPIO_HW Hw, ULONG Pin)
{
    ULONG  off;
    int    b = Rp1GpioPinToBank(Pin, &off);
    PUCHAR inte;
    if (b < 0 || Hw->GpioBase == NULL) {
        return;
    }
    inte = BankReg(Hw, b, RP1_GPIO_PCIE_INTE);
    WRITE_REGISTER_ULONG((volatile ULONG *)inte,
        READ_REGISTER_ULONG((volatile ULONG *)inte) | (1u << off));
}

ULONG Rp1GpioQueryActiveIrq(PRP1_GPIO_HW Hw, int Bank)
{
    if (Bank < 0 || Bank > 2 || Hw->GpioBase == NULL) {
        return 0;
    }
    return READ_REGISTER_ULONG((volatile ULONG *)BankReg(Hw, Bank, RP1_GPIO_PCIE_INTS));
}

VOID Rp1GpioAckIrq(PRP1_GPIO_HW Hw, ULONG Pin)
{
    ULONG  off;
    int    b = Rp1GpioPinToBank(Pin, &off);
    PUCHAR ctrl;
    ULONG  v;
    if (b < 0 || Hw->GpioBase == NULL) {
        return;
    }
    ctrl = PinCtrl(Hw, b, off);
    v = READ_REGISTER_ULONG((volatile ULONG *)ctrl);
    WRITE_REGISTER_ULONG((volatile ULONG *)ctrl, v | RP1_GPIO_CTRL_IRQRESET);  /* latch reset */
}
