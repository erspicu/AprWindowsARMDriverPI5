/*++
Module Name: common.h
Abstract:    BCM2712 VideoCore mailbox - KMDF driver. Maps the mailbox MMIO; the
             property/clock/power channel protocol is driven via mbox_hw.c. Other
             RP1/BCM subsystems (clocks, power) depend on this.
--*/
#pragma once
#include <ntddk.h>
#include <wdf.h>
#include "mbox_hw.h"

#define BCMMBOX_TAG  'bMcB'   /* 'BcMb' */

typedef struct _BCMMBOX_CONTEXT {
    PVOID  Base;
    SIZE_T Length;
} BCMMBOX_CONTEXT, *PBCMMBOX_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BCMMBOX_CONTEXT, BcmMboxGetContext)

EVT_WDF_DRIVER_DEVICE_ADD       BcmMboxEvtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE BcmMboxEvtPrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE BcmMboxEvtReleaseHardware;
