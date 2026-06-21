/*++
Module Name: dma_hw.h
Abstract:    BCM2835/BCM2712 DMA HAL. Per-channel control-block engine, ported
             from Linux drivers/dma/bcm2835-dma.c. Pure logic, sim-able.
--*/
#pragma once

#define BCM2835_DMA_CS          0x00
#define BCM2835_DMA_ADDR        0x04   /* control-block (CONBLK_AD) */
#define BCM2835_DMA_SOURCE_AD   0x0c
#define BCM2835_DMA_DEST_AD     0x10
#define BCM2835_DMA_LEN         0x14
#define BCM2835_DMA_NEXTCB      0x1c
#define BCM2835_DMA_INT_STATUS  0xfe0
#define BCM2835_DMA_ENABLE      0xff0
#define BCM2835_DMA_CHAN_SIZE   0x100

#define BCM2835_DMA_ACTIVE      (1u << 0)
#define BCM2835_DMA_END         (1u << 1)
#define BCM2835_DMA_INT         (1u << 2)
#define BCM2835_DMA_RESET       (1u << 31)   /* WO, self-clearing */
#define BCM2835_DMA_CHANNELS    16
#define DMA_CHAN_BASE(ch)       ((ch) * BCM2835_DMA_CHAN_SIZE)

/* ---- BCM2711/BCM2712 DMA40 (40-bit) engine. The Pi5 channels are 40-bit and
 * use a DIFFERENT register map + sequences than legacy bcm2835 (ported from the
 * is_40bit_channel paths in bcm2835-dma.c). The control-block address is the
 * physical address >> 5 (to_40bit_cbaddr). ---- */
#define BCM2711_DMA40_CS        0x00
#define BCM2711_DMA40_CB        0x04   /* control block = paddr >> 5 */
#define BCM2711_DMA40_DEBUG     0x0c
#define BCM2711_DMA40_TI        0x10
#define BCM2711_DMA40_SRC       0x14
#define BCM2711_DMA40_DEST      0x1c
#define BCM2711_DMA40_LEN       0x24
#define BCM2711_DMA40_NEXT_CB   0x28

#define BCM2711_DMA40_ACTIVE       (1u << 0)
#define BCM2711_DMA40_END          (1u << 1)
#define BCM2711_DMA40_INT          (1u << 2)
#define BCM2711_DMA40_PROT         ((1u << 8) | (1u << 9))
#define BCM2711_DMA40_TRANSACTIONS (1u << 25)
#define BCM2711_DMA40_DISDEBUG     (1u << 29)
#define BCM2711_DMA40_ABORT        (1u << 30)
#define BCM2711_DMA40_HALT         (1u << 31)
#define BCM2711_DMA40_DEBUG_RESET  (1u << 23)
#define BCM2711_DMA40_CS_FLAGS(dreq) ((dreq) & 0x00fff800u)   /* QOS|PANIC|WAITWR|DREQ etc. */
#define DMA40_TO_CBADDR(paddr64)   ((unsigned)(((unsigned long long)(paddr64)) >> 5))

/* legacy bcm2835 path */
void DmaHwReset(void *Base, unsigned Channel);
void DmaHwEnable(void *Base, unsigned Channel);
void DmaHwStart(void *Base, unsigned Channel, unsigned CbAddr);
int  DmaHwIsActive(void *Base, unsigned Channel);
int  DmaHwIsDone(void *Base, unsigned Channel);
void DmaHwAckInt(void *Base, unsigned Channel);

/* BCM2712 40-bit path (CbAddr40 = physical control-block address >> 5) */
void DmaHw40Reset(void *Base, unsigned Channel);
void DmaHw40Start(void *Base, unsigned Channel, unsigned CbAddr40, unsigned Dreq);
int  DmaHw40IsActive(void *Base, unsigned Channel);
int  DmaHw40IsDone(void *Base, unsigned Channel);
void DmaHw40AckInt(void *Base, unsigned Channel, unsigned Dreq);
