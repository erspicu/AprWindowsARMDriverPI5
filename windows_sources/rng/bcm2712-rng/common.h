/*++
Module Name: common.h
Abstract:    BCM2712 iProc RNG200 hardware RNG - KMDF driver. Maps the RNG MMIO,
             enables the RBG; words are drained via rng_hw.c. (A full driver
             would feed the Windows CNG entropy pool - left as TODO.)
--*/
#pragma once
#include <ntddk.h>
#include <wdf.h>
#include "rng_hw.h"

#define BCMRNG_TAG  'gnRB'   /* 'BRng' */

typedef struct _BCMRNG_CONTEXT {
    PVOID  Base;
    SIZE_T Length;
} BCMRNG_CONTEXT, *PBCMRNG_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BCMRNG_CONTEXT, BcmRngGetContext)

EVT_WDF_DRIVER_DEVICE_ADD       BcmRngEvtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE BcmRngEvtPrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE BcmRngEvtReleaseHardware;
