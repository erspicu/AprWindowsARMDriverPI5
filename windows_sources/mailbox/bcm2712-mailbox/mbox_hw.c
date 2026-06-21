/*++ mbox_hw.c - VideoCore mailbox HAL (ported from bcm2835-mailbox.c). --*/
#include "mbox_regio.h"
#include "mbox_hw.h"

#define MBOX_POLL_MAX 100000

int MboxHwIsFull(void *Base)  { return (RD32(Base, MAIL1_STA) & ARM_MS_FULL)  ? 1 : 0; }
int MboxHwIsEmpty(void *Base) { return (RD32(Base, MAIL0_STA) & ARM_MS_EMPTY) ? 1 : 0; }

int MboxHwSend(void *Base, unsigned Data, unsigned Channel)
{
    unsigned i;
    for (i = 0; i < MBOX_POLL_MAX; i++) {
        if (!(RD32(Base, MAIL1_STA) & ARM_MS_FULL)) {
            break;
        }
    }
    if (i == MBOX_POLL_MAX) {
        return -1;
    }
    WR32(Base, MAIL1_WRT, (Data & ~MBOX_CHAN_MASK) | (Channel & MBOX_CHAN_MASK));
    return 0;
}

int MboxHwRecv(void *Base, unsigned *OutData, unsigned *OutChannel)
{
    unsigned i, v;
    for (i = 0; i < MBOX_POLL_MAX; i++) {
        if (!(RD32(Base, MAIL0_STA) & ARM_MS_EMPTY)) {
            break;
        }
    }
    if (i == MBOX_POLL_MAX) {
        return -1;
    }
    v = RD32(Base, MAIL0_RD);
    *OutData    = v & ~MBOX_CHAN_MASK;
    *OutChannel = v & MBOX_CHAN_MASK;
    return 0;
}
