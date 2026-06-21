/*++
Module Name: common.h
Abstract:    BCM2712 / brcmstb GPIO controller (GpioClx). Same GpioClx structure
             as the RP1 GPIO driver (#9); the HAL is brcm_gpio_hw.c (GIO banks).
--*/
#pragma once
#include <ntddk.h>
#include <wdf.h>
#include <gpioclx.h>
#include "brcm_gpio_hw.h"

#define BCMGPIO_POOLTAG       'gGcB'   /* 'BcGg' */
#define BCM_GPIO_TOTAL_PINS   (BCM_GPIO_BANKS * BCM_GPIO_PINS_PER_BANK)  /* 64 */

typedef struct _BCMGPIO_CONTEXT {
    PVOID  Base;
    SIZE_T Length;
} BCMGPIO_CONTEXT, *PBCMGPIO_CONTEXT;

EVT_WDF_DRIVER_DEVICE_ADD BcmGpioEvtDeviceAdd;
