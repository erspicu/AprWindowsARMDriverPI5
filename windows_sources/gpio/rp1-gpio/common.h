/*++
Module Name: common.h
Abstract:    RP1 GPIO controller driver (GpioClx) - scaffold.
             Registers with the inbox GpioClx framework so Windows exposes the
             RP1 GPIO pins (read/write/config) and routes pin interrupts. This
             is also the home of the RP1 interrupt demux (MSI-X -> GpioInt) that
             the rp1bus children depend on.
--*/
#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <gpioclx.h>
#include "rp1_gpio_hw.h"

#define RP1GPIO_POOLTAG  'pGpR'   // 'RpGp'
#define RP1_GPIO_PIN_COUNT  54    // RP1 GPIO0..53

typedef struct _RP1GPIO_CONTEXT {
    RP1_GPIO_HW Hw;     // GpioBase / RioBase / PadsBase
    ULONG       PinCount;
} RP1GPIO_CONTEXT, *PRP1GPIO_CONTEXT;

EVT_WDF_DRIVER_DEVICE_ADD Rp1GpioEvtDeviceAdd;
