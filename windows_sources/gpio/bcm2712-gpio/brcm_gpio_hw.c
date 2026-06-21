/*++ brcm_gpio_hw.c - BCM2712/brcmstb GPIO HAL (ported from gpio-brcmstb.c). --*/
#include "brcm_regio.h"
#include "brcm_gpio_hw.h"

static void SetBit(void *Base, unsigned Off, unsigned Bit, int Set)
{
    unsigned v = RD32(Base, Off);
    if (Set) {
        v |= (1u << Bit);
    } else {
        v &= ~(1u << Bit);
    }
    WR32(Base, Off, v);
}

int BcmGpioReadPin(void *Base, unsigned Bank, unsigned Pin)
{
    return (RD32(Base, GIO_BANK_OFF(Bank, GIO_REG_DATA)) >> Pin) & 1u;
}

void BcmGpioWritePin(void *Base, unsigned Bank, unsigned Pin, int Value)
{
    SetBit(Base, GIO_BANK_OFF(Bank, GIO_REG_DATA), Pin, Value);
}

void BcmGpioSetDirection(void *Base, unsigned Bank, unsigned Pin, int Output)
{
    /* IODIR bit: 1 = input, 0 = output */
    SetBit(Base, GIO_BANK_OFF(Bank, GIO_REG_IODIR), Pin, Output ? 0 : 1);
}

void BcmGpioEnableIrq(void *Base, unsigned Bank, unsigned Pin, int Edge, int BothEdges, int High)
{
    SetBit(Base, GIO_BANK_OFF(Bank, GIO_REG_EC),    Pin, Edge);       /* edge vs level */
    SetBit(Base, GIO_BANK_OFF(Bank, GIO_REG_EI),    Pin, BothEdges);  /* both edges */
    SetBit(Base, GIO_BANK_OFF(Bank, GIO_REG_LEVEL), Pin, High);       /* polarity */
    SetBit(Base, GIO_BANK_OFF(Bank, GIO_REG_MASK),  Pin, 1);          /* enable */
}

void BcmGpioMaskIrq(void *Base, unsigned Bank, unsigned Pin)
{
    SetBit(Base, GIO_BANK_OFF(Bank, GIO_REG_MASK), Pin, 0);
}

void BcmGpioUnmaskIrq(void *Base, unsigned Bank, unsigned Pin)
{
    SetBit(Base, GIO_BANK_OFF(Bank, GIO_REG_MASK), Pin, 1);
}

unsigned BcmGpioQueryActive(void *Base, unsigned Bank)
{
    return RD32(Base, GIO_BANK_OFF(Bank, GIO_REG_STAT)) &
           RD32(Base, GIO_BANK_OFF(Bank, GIO_REG_MASK));
}

void BcmGpioAckIrq(void *Base, unsigned Bank, unsigned Pin)
{
    WR32(Base, GIO_BANK_OFF(Bank, GIO_REG_STAT), (1u << Pin));   /* write-1-to-clear */
}
