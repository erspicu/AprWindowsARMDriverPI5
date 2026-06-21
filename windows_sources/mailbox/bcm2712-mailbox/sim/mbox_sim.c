/*++ mbox_sim.c - x64 simulation of the VideoCore mailbox HAL. Build: cl /DMBOX_SIM /I.. mbox_sim.c ..\mbox_hw.c --*/
#include <stdio.h>
#include <string.h>
#include "../mbox_hw.h"

unsigned char *g_mboxBase;
static unsigned char g_regs[0x40];
static unsigned load(unsigned off)         { unsigned v=0; int i; for(i=0;i<4;i++) v|=((unsigned)g_regs[off+i])<<(8*i); return v; }
static void     store(unsigned off,unsigned v){ int i; for(i=0;i<4;i++) g_regs[off+i]=(unsigned char)((v>>(8*i))&0xFF); }
unsigned int MboxSimRd(unsigned off)        { return load(off); }
void         MboxSimWr(unsigned off, unsigned val){ store(off, val); }

static int g_pass, g_fail;
static void check(const char *what, int c){ if(c){g_pass++;printf("  [PASS] %s\n",what);}else{g_fail++;printf("  [FAIL] %s\n",what);} }

int main(void)
{
    int rc;
    unsigned data, chan;

    memset(g_regs, 0, sizeof(g_regs));
    g_mboxBase = g_regs;
    printf("== VideoCore mailbox HAL simulation ==\n");

    /* MAIL1_STA = 0 (not full) -> send proceeds immediately */
    rc = MboxHwSend(g_regs, 0x12345670, 8);  /* channel 8 = property tags */
    check("Send returns 0", rc == 0);
    check("MAIL1_WRT == payload | channel (0x12345678)", load(MAIL1_WRT) == 0x12345678u);

    /* full status */
    store(MAIL1_STA, ARM_MS_FULL);
    check("IsFull true when STA has FULL", MboxHwIsFull(g_regs) == 1);
    store(MAIL1_STA, 0);
    check("IsFull false otherwise", MboxHwIsFull(g_regs) == 0);

    /* recv: MAIL0_STA = 0 (not empty), MAIL0_RD carries data|channel */
    store(MAIL0_STA, 0);
    store(MAIL0_RD, 0xABCDEF09);             /* channel 9, payload 0xABCDEF00 */
    rc = MboxHwRecv(g_regs, &data, &chan);
    check("Recv returns 0", rc == 0);
    check("recv channel == 9", chan == 9);
    check("recv payload == 0xABCDEF00", data == 0xABCDEF00u);

    /* empty status */
    store(MAIL0_STA, ARM_MS_EMPTY);
    check("IsEmpty true when STA has EMPTY", MboxHwIsEmpty(g_regs) == 1);

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
