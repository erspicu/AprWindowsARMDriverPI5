/*++
Module Name: common.h
Abstract:    BCM2712 DMA controller - KMDF driver. Maps the DMA MMIO; channel
             reset/enable/start via dma_hw.c. (Other drivers would request DMA
             channels via an internal interface - left as TODO.)
--*/
#pragma once
#include <ntddk.h>
#include <wdf.h>
#include "dma_hw.h"

#define BCMDMA_TAG  'aMdB'   /* 'BdMa' */

typedef struct _BCMDMA_CONTEXT {
    PVOID  Base;
    SIZE_T Length;
} BCMDMA_CONTEXT, *PBCMDMA_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BCMDMA_CONTEXT, BcmDmaGetContext)

EVT_WDF_DRIVER_DEVICE_ADD       BcmDmaEvtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE BcmDmaEvtPrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE BcmDmaEvtReleaseHardware;
