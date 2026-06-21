/*++

Module Name:
    driver.h

Abstract:
    RP1 I2S - Phase 1 WDM function-driver skeleton (ARM64).

    This is a plain WDM PnP function driver that:
      - attaches to the device stack (AddDevice),
      - on IRP_MN_START_DEVICE parses the translated CM resource list, maps the
        I2S MMIO window, and drives the ported HAL (rp1_i2s_hw.c),
      - passes all other PnP/Power IRPs down the stack.

    WDM (not KMDF) is used on purpose: it has the fewest link-time dependencies
    for a hand-linked ARM64 build, and it is the SAME driver model that the
    eventual PortCls/WaveRT audio driver (Phase 2) is built on.

--*/

#pragma once

#include <ntddk.h>
#include "rp1_i2s_hw.h"

#define RP1_POOL_TAG  'S2IR'   /* 'RI2S' shown in pool tags */

typedef struct _RP1I2S_DEVICE_EXTENSION {
    PDEVICE_OBJECT   Self;          /* our FDO                                  */
    PDEVICE_OBJECT   LowerDevice;   /* device returned by AttachToDeviceStack   */
    PDEVICE_OBJECT   Pdo;           /* the physical device object               */
    IO_REMOVE_LOCK   RemoveLock;
    RP1I2S_HW        Hw;            /* ported DesignWare I2S HAL state           */
    PUCHAR           MmioBase;      /* mapped I2S register window                */
    SIZE_T           MmioLength;
    PHYSICAL_ADDRESS MmioPhys;
    BOOLEAN          Started;
} RP1I2S_DEVICE_EXTENSION, *PRP1I2S_DEVICE_EXTENSION;

DRIVER_INITIALIZE DriverEntry;
DRIVER_ADD_DEVICE Rp1I2sAddDevice;
DRIVER_UNLOAD     Rp1I2sUnload;

DRIVER_DISPATCH   Rp1I2sDispatchPnp;
DRIVER_DISPATCH   Rp1I2sDispatchPower;
DRIVER_DISPATCH   Rp1I2sDispatchSystemControl;
DRIVER_DISPATCH   Rp1I2sDispatchCreateClose;

NTSTATUS Rp1I2sStartDevice(_Inout_ PRP1I2S_DEVICE_EXTENSION Ext,
                           _In_opt_ PCM_RESOURCE_LIST Translated);
VOID     Rp1I2sStopDevice(_Inout_ PRP1I2S_DEVICE_EXTENSION Ext);
