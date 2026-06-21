/*++ dma_hw.c - BCM2835/BCM2712 DMA HAL (ported from bcm2835-dma.c). --*/
#include "dma_regio.h"
#include "dma_hw.h"

#define DMA_POLL_MAX 100000

void DmaHwReset(void *Base, unsigned Channel)
{
    unsigned c = DMA_CHAN_BASE(Channel);
    unsigned i;
    WR32(Base, c + BCM2835_DMA_CS, BCM2835_DMA_RESET);
    for (i = 0; i < DMA_POLL_MAX; i++) {
        if (!(RD32(Base, c + BCM2835_DMA_CS) & BCM2835_DMA_RESET)) {
            break;
        }
    }
}

void DmaHwEnable(void *Base, unsigned Channel)
{
    unsigned v = RD32(Base, BCM2835_DMA_ENABLE);
    WR32(Base, BCM2835_DMA_ENABLE, v | (1u << Channel));
}

void DmaHwStart(void *Base, unsigned Channel, unsigned CbAddr)
{
    unsigned c = DMA_CHAN_BASE(Channel);
    WR32(Base, c + BCM2835_DMA_ADDR, CbAddr);          /* control-block address */
    WR32(Base, c + BCM2835_DMA_CS, BCM2835_DMA_ACTIVE); /* go */
}

int DmaHwIsActive(void *Base, unsigned Channel)
{
    return (RD32(Base, DMA_CHAN_BASE(Channel) + BCM2835_DMA_CS) & BCM2835_DMA_ACTIVE) ? 1 : 0;
}

int DmaHwIsDone(void *Base, unsigned Channel)
{
    return (RD32(Base, DMA_CHAN_BASE(Channel) + BCM2835_DMA_CS) & BCM2835_DMA_END) ? 1 : 0;
}

void DmaHwAckInt(void *Base, unsigned Channel)
{
    unsigned c = DMA_CHAN_BASE(Channel);
    WR32(Base, c + BCM2835_DMA_CS, BCM2835_DMA_END | BCM2835_DMA_INT);  /* write-1-to-clear */
}

/* ===== BCM2712 DMA40 (40-bit) path (bcm2835-dma.c is_40bit_channel) ===== */

/* Reset: pause (clear ACTIVE), wait for outstanding transactions, restore CS to
 * the PROT default, then pulse DEBUG_RESET. (The legacy CS-bit reset is wrong for
 * a 40-bit channel.) Skips an idle channel (CB == 0). */
void DmaHw40Reset(void *Base, unsigned Channel)
{
    unsigned c = DMA_CHAN_BASE(Channel);
    unsigned i;
    if (RD32(Base, c + BCM2711_DMA40_CB) == 0) {
        return;                                 /* idle: zero CB address */
    }
    WR32(Base, c + BCM2711_DMA40_CS,
         RD32(Base, c + BCM2711_DMA40_CS) & ~BCM2711_DMA40_ACTIVE);   /* pause */
    for (i = 0; i < DMA_POLL_MAX; i++) {
        if (!(RD32(Base, c + BCM2711_DMA40_CS) & BCM2711_DMA40_TRANSACTIONS)) {
            break;
        }
    }
    WR32(Base, c + BCM2711_DMA40_CS, BCM2711_DMA40_PROT);             /* default state */
    WR32(Base, c + BCM2711_DMA40_DEBUG,
         RD32(Base, c + BCM2711_DMA40_DEBUG) | BCM2711_DMA40_DEBUG_RESET);
}

/* Start: write the >>5 control-block address, then go with ACTIVE|PROT|flags. */
void DmaHw40Start(void *Base, unsigned Channel, unsigned CbAddr40, unsigned Dreq)
{
    unsigned c = DMA_CHAN_BASE(Channel);
    WR32(Base, c + BCM2711_DMA40_CB, CbAddr40);
    WR32(Base, c + BCM2711_DMA40_CS,
         BCM2711_DMA40_ACTIVE | BCM2711_DMA40_PROT | BCM2711_DMA40_CS_FLAGS(Dreq));
}

int DmaHw40IsActive(void *Base, unsigned Channel)
{
    return (RD32(Base, DMA_CHAN_BASE(Channel) + BCM2711_DMA40_CS) & BCM2711_DMA40_ACTIVE) ? 1 : 0;
}

int DmaHw40IsDone(void *Base, unsigned Channel)
{
    return (RD32(Base, DMA_CHAN_BASE(Channel) + BCM2711_DMA40_CS) & BCM2711_DMA40_END) ? 1 : 0;
}

/* Ack: write-1-clear INT but KEEP the channel active (ACTIVE|PROT|flags) so a
 * cyclic descriptor keeps running (legacy ack cleared ACTIVE, breaking cyclic). */
void DmaHw40AckInt(void *Base, unsigned Channel, unsigned Dreq)
{
    unsigned c = DMA_CHAN_BASE(Channel);
    WR32(Base, c + BCM2711_DMA40_CS,
         BCM2711_DMA40_INT | BCM2711_DMA40_ACTIVE | BCM2711_DMA40_PROT |
         BCM2711_DMA40_CS_FLAGS(Dreq));
}
