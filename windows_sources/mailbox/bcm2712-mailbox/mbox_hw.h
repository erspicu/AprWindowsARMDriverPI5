/*++
Module Name: mbox_hw.h
Abstract:    BCM2835/BCM2712 VideoCore mailbox HAL. Register map + send/recv,
             ported from Linux drivers/mailbox/bcm2835-mailbox.c. The low 4 bits
             of each 32-bit message select the channel; the upper 28 are payload.
--*/
#pragma once

#define MAIL0_RD      0x00
#define MAIL0_STA     0x18
#define MAIL1_WRT     0x20
#define MAIL1_STA     0x38
#define ARM_MS_FULL   (1u << 31)
#define ARM_MS_EMPTY  (1u << 30)
#define MBOX_CHAN_MASK 0x0Fu

int MboxHwIsFull(void *Base);
int MboxHwIsEmpty(void *Base);
int MboxHwSend(void *Base, unsigned Data, unsigned Channel);              /* 0 ok, -1 timeout */
int MboxHwRecv(void *Base, unsigned *OutData, unsigned *OutChannel);      /* 0 ok, -1 timeout */
