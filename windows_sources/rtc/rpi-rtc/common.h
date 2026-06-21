/*++
Module Name: common.h
Abstract:    Raspberry Pi RTC (firmware-backed) - KMDF skeleton. The Pi5 RTC has
             no MMIO of its own: get/set time goes through the VideoCore firmware
             property channel (the mailbox driver #16). This skeleton stands up
             the KMDF device; the firmware GET_RTC/SET_RTC property exchange is
             the next stage (built on bcmmbox).
--*/
#pragma once
#include <ntddk.h>
#include <wdf.h>

#define RPIRTC_TAG  'tRpR'   /* 'RpRt' */

typedef struct _RPIRTC_CONTEXT {
    ULONG Reserved;   /* no MMIO; time is proxied via the firmware mailbox */
} RPIRTC_CONTEXT, *PRPIRTC_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(RPIRTC_CONTEXT, RpiRtcGetContext)

EVT_WDF_DRIVER_DEVICE_ADD RpiRtcEvtDeviceAdd;
