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
#include "rp1bus_if.h"        // bus interface exported by rp1bus.sys (parent)

#define RP1GPIO_POOLTAG  'pGpR'   // 'RpGp'
#define RP1_GPIO_PIN_COUNT  54    // RP1 GPIO0..53

// When enumerated as an rp1bus child (RP1\GPIO0), the parent hands us one BAR1
// window covering all three GPIO sub-blocks. Sub-offsets within that window
// (from Pi5 rp1.dtsi: io 0x40d0000 / rio 0x40e0000 / pads 0x40f0000, each 0xc000).
#define RP1_GPIO_IO_SUBOFF    0x00000
#define RP1_GPIO_RIO_SUBOFF   0x10000
#define RP1_GPIO_PADS_SUBOFF  0x20000
#define RP1_GPIO_SUBLEN       0xC000

typedef struct _RP1GPIO_CONTEXT {
    RP1_GPIO_HW               Hw;       // GpioBase / RioBase / PadsBase
    ULONG                     PinCount;
    BOOLEAN                   FromBusIf; // window came from rp1bus interface (don't unmap)
    RP1BUS_INTERFACE_STANDARD BusIf;     // held ref while we use the window
} RP1GPIO_CONTEXT, *PRP1GPIO_CONTEXT;

EVT_WDF_DRIVER_DEVICE_ADD Rp1GpioEvtDeviceAdd;
