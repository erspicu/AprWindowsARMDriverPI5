/*++
Module Name: pio_hw.h
Abstract:    RP1 PIO HAL - direct TX/RX FIFO access for the 4 state machines.
             Ported from Linux drivers/misc/rp1-pio.c. NOTE: state-machine
             programming (instructions, clkdiv, pinctrl) goes through the RP1
             firmware mailbox, not direct registers - that is the next stage.
--*/
#pragma once

#define RP1_PIO_SMS_COUNT   4
#define RP1_PIO_FIFO_DEPTH  8
#define RP1_PIO_FIFO_TX(sm) (0x00 + (sm) * 4)   /* TX0..TX3 = 0x00,0x04,0x08,0x0c */
#define RP1_PIO_FIFO_RX(sm) (0x10 + (sm) * 4)   /* RX0..RX3 = 0x10,0x14,0x18,0x1c */
#define RP1_PIO_DMACTRL_DEFAULT 0x80000100u

void     PioHwPushTx(void *Base, unsigned Sm, unsigned Value);
unsigned PioHwPopRx(void *Base, unsigned Sm);
