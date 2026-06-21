/*++
Module Name: common.h
Abstract:    RP1 PIO - KMDF driver. Maps the PIO MMIO; TX/RX FIFO access via
             pio_hw.c. State-machine programs are loaded via the RP1 firmware
             mailbox (#16) - that protocol layer is the next stage.
--*/
#pragma once
#include <ntddk.h>
#include <wdf.h>
#include "pio_hw.h"

#define RP1PIO_TAG  'oIpR'   /* 'RpIo' */

typedef struct _RP1PIO_CONTEXT {
    PVOID  Base;
    SIZE_T Length;
} RP1PIO_CONTEXT, *PRP1PIO_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(RP1PIO_CONTEXT, Rp1PioGetContext)

EVT_WDF_DRIVER_DEVICE_ADD       Rp1PioEvtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE Rp1PioEvtPrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE Rp1PioEvtReleaseHardware;
