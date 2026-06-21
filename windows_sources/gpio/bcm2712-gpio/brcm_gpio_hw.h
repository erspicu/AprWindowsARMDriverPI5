/*++
Module Name: brcm_gpio_hw.h
Abstract:    BCM2712 / brcmstb GPIO HAL. Per-bank GIO registers, ported from
             Linux drivers/gpio/gpio-brcmstb.c. Pure logic, sim-able.
--*/
#pragma once

/* GIO register indices (per bank, stride GIO_BANK_SIZE) */
#define GIO_REG_ODEN   0
#define GIO_REG_DATA   1
#define GIO_REG_IODIR  2   /* 1 = input, 0 = output */
#define GIO_REG_EC     3   /* edge config (1 = edge, 0 = level) */
#define GIO_REG_EI     4   /* edge insensitive (1 = both edges) */
#define GIO_REG_MASK   5   /* interrupt mask (1 = enabled) */
#define GIO_REG_LEVEL  6   /* polarity (1 = high/rising) */
#define GIO_REG_STAT   7   /* interrupt status (W1C) */
#define GIO_NUM_REGS   8
#define GIO_BANK_SIZE  (GIO_NUM_REGS * 4)
#define GIO_BANK_OFF(bank, regidx) ((bank) * GIO_BANK_SIZE + (regidx) * 4)

#define BCM_GPIO_PINS_PER_BANK 32
#define BCM_GPIO_BANKS         2

int      BcmGpioReadPin(void *Base, unsigned Bank, unsigned Pin);
void     BcmGpioWritePin(void *Base, unsigned Bank, unsigned Pin, int Value);
void     BcmGpioSetDirection(void *Base, unsigned Bank, unsigned Pin, int Output);
void     BcmGpioEnableIrq(void *Base, unsigned Bank, unsigned Pin, int Edge, int BothEdges, int High);
void     BcmGpioMaskIrq(void *Base, unsigned Bank, unsigned Pin);
void     BcmGpioUnmaskIrq(void *Base, unsigned Bank, unsigned Pin);
unsigned BcmGpioQueryActive(void *Base, unsigned Bank);   /* STAT & MASK */
void     BcmGpioAckIrq(void *Base, unsigned Bank, unsigned Pin);
