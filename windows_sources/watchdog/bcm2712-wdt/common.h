/*++
Module Name: common.h
Abstract:    BCM2712 PM watchdog - KMDF driver. Maps the PM watchdog MMIO; arm/
             ping/stop via wdt_hw.c. (A full driver registers with the Windows
             watchdog stack / WDF watchdog - left as TODO.)
--*/
#pragma once
#include <ntddk.h>
#include <wdf.h>
#include "wdt_hw.h"

#define BCMWDT_TAG  'tWcB'   /* 'BcWt' */

typedef struct _BCMWDT_CONTEXT {
    PVOID  Base;
    SIZE_T Length;
} BCMWDT_CONTEXT, *PBCMWDT_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BCMWDT_CONTEXT, BcmWdtGetContext)

EVT_WDF_DRIVER_DEVICE_ADD       BcmWdtEvtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE BcmWdtEvtPrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE BcmWdtEvtReleaseHardware;
