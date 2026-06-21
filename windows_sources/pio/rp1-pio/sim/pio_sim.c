/*++ pio_sim.c - x64 simulation of the RP1 PIO FIFO HAL. Build: cl /DPIO_SIM /I.. pio_sim.c ..\pio_hw.c --*/
#include <stdio.h>
#include <string.h>
#include "../pio_hw.h"

unsigned char *g_pioBase;
static unsigned char g_regs[0x40];
static unsigned load(unsigned off)         { unsigned v=0; int i; for(i=0;i<4;i++) v|=((unsigned)g_regs[off+i])<<(8*i); return v; }
static void     store(unsigned off,unsigned v){ int i; for(i=0;i<4;i++) g_regs[off+i]=(unsigned char)((v>>(8*i))&0xFF); }
unsigned int PioSimRd(unsigned off)        { return load(off); }
void         PioSimWr(unsigned off, unsigned val){ store(off, val); }

static int g_pass, g_fail;
static void check(const char *what, int c){ if(c){g_pass++;printf("  [PASS] %s\n",what);}else{g_fail++;printf("  [FAIL] %s\n",what);} }

int main(void)
{
    unsigned sm;
    memset(g_regs, 0, sizeof(g_regs));
    g_pioBase = g_regs;
    printf("== RP1 PIO FIFO HAL simulation ==\n");

    /* push to each SM's TX FIFO -> distinct offsets 0x00/0x04/0x08/0x0c */
    for (sm = 0; sm < RP1_PIO_SMS_COUNT; sm++) {
        PioHwPushTx(g_pioBase, sm, 0x1000 + sm);
    }
    check("TX0 @0x00 == 0x1000", load(RP1_PIO_FIFO_TX(0)) == 0x1000);
    check("TX1 @0x04 == 0x1001", load(RP1_PIO_FIFO_TX(1)) == 0x1001);
    check("TX2 @0x08 == 0x1002", load(RP1_PIO_FIFO_TX(2)) == 0x1002);
    check("TX3 @0x0c == 0x1003", load(RP1_PIO_FIFO_TX(3)) == 0x1003);

    /* preset RX FIFOs (0x10/0x14/0x18/0x1c) and pop */
    store(RP1_PIO_FIFO_RX(0), 0xAA00);
    store(RP1_PIO_FIFO_RX(3), 0xAA03);
    check("RX0 @0x10 popped == 0xAA00", PioHwPopRx(g_pioBase, 0) == 0xAA00);
    check("RX3 @0x1c popped == 0xAA03", PioHwPopRx(g_pioBase, 3) == 0xAA03);

    check("TX/RX FIFO regions distinct (no overlap)", RP1_PIO_FIFO_RX(0) > RP1_PIO_FIFO_TX(3));

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
