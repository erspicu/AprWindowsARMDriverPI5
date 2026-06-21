/*++ brcm_gpio_sim.c - x64 simulation of the BCM2712 GPIO HAL. Build: cl /DBRCMGPIO_SIM /I.. brcm_gpio_sim.c ..\brcm_gpio_hw.c --*/
#include <stdio.h>
#include <string.h>
#include "../brcm_gpio_hw.h"

unsigned char *g_brcmBase;
static unsigned char g_regs[0x100];
static unsigned load(unsigned off)         { unsigned v=0; int i; for(i=0;i<4;i++) v|=((unsigned)g_regs[off+i])<<(8*i); return v; }
static void     store(unsigned off,unsigned v){ int i; for(i=0;i<4;i++) g_regs[off+i]=(unsigned char)((v>>(8*i))&0xFF); }
unsigned int BrcmSimRd(unsigned off)        { return load(off); }
void         BrcmSimWr(unsigned off, unsigned val){ store(off, val); }

static int g_pass, g_fail;
static void check(const char *what, int c){ if(c){g_pass++;printf("  [PASS] %s\n",what);}else{g_fail++;printf("  [FAIL] %s\n",what);} }

int main(void)
{
    memset(g_regs, 0, sizeof(g_regs));
    g_brcmBase = g_regs;
    printf("== BCM2712 (brcmstb) GPIO HAL simulation ==\n");

    BcmGpioWritePin(g_regs, 0, 5, 1);
    check("bank0 DATA(@0x04) bit5 set", (load(GIO_BANK_OFF(0,GIO_REG_DATA)) & (1u<<5)) != 0);
    store(GIO_BANK_OFF(0,GIO_REG_DATA), 1u<<10);
    check("ReadPin bank0 pin10 == 1", BcmGpioReadPin(g_regs, 0, 10) == 1);

    BcmGpioSetDirection(g_regs, 0, 3, 1);   /* output -> IODIR bit clear */
    check("IODIR(@0x08) bit3 == 0 (output)", (load(GIO_BANK_OFF(0,GIO_REG_IODIR)) & (1u<<3)) == 0);
    BcmGpioSetDirection(g_regs, 0, 3, 0);   /* input -> IODIR bit set */
    check("IODIR bit3 == 1 (input)", (load(GIO_BANK_OFF(0,GIO_REG_IODIR)) & (1u<<3)) != 0);

    /* rising-edge interrupt on pin 7: EC=1, EI=0, LEVEL=1, MASK=1 */
    BcmGpioEnableIrq(g_regs, 0, 7, 1, 0, 1);
    check("EC bit7 set (edge)",    (load(GIO_BANK_OFF(0,GIO_REG_EC))    & (1u<<7)) != 0);
    check("EI bit7 clear (single)",(load(GIO_BANK_OFF(0,GIO_REG_EI))    & (1u<<7)) == 0);
    check("LEVEL bit7 set (rising)",(load(GIO_BANK_OFF(0,GIO_REG_LEVEL))& (1u<<7)) != 0);
    check("MASK bit7 set (enabled)",(load(GIO_BANK_OFF(0,GIO_REG_MASK)) & (1u<<7)) != 0);

    /* active = STAT & MASK */
    store(GIO_BANK_OFF(0,GIO_REG_STAT), (1u<<7) | (1u<<9));   /* pin9 not masked */
    check("QueryActive bank0 == pin7 only (masked by MASK)", BcmGpioQueryActive(g_regs, 0) == (1u<<7));
    BcmGpioAckIrq(g_regs, 0, 7);
    check("AckIrq wrote STAT bit7 (W1C)", (load(GIO_BANK_OFF(0,GIO_REG_STAT)) & (1u<<7)) != 0);

    BcmGpioMaskIrq(g_regs, 0, 7);
    check("Mask clears MASK bit7", (load(GIO_BANK_OFF(0,GIO_REG_MASK)) & (1u<<7)) == 0);

    /* bank 1 at +0x20 */
    BcmGpioWritePin(g_regs, 1, 2, 1);
    check("bank1 DATA(@0x24) bit2 set", (load(GIO_BANK_OFF(1,GIO_REG_DATA)) & (1u<<2)) != 0);

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
