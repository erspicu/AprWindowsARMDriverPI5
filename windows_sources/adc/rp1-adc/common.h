/*++
Module Name: common.h
Abstract:    RP1 ADC + temp sensor - KMDF driver. Maps the ADC MMIO; one-shot
             conversions via adc_hw.c. (Exposed via driver IOCTLs - TODO.)
--*/
#pragma once
#include <ntddk.h>
#include <wdf.h>
#include "adc_hw.h"

#define RP1ADC_TAG  'cAcR'   /* 'RcAc' */

typedef struct _RP1ADC_CONTEXT {
    PVOID  Base;
    SIZE_T Length;
} RP1ADC_CONTEXT, *PRP1ADC_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(RP1ADC_CONTEXT, Rp1AdcGetContext)

EVT_WDF_DRIVER_DEVICE_ADD       Rp1AdcEvtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE Rp1AdcEvtPrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE Rp1AdcEvtReleaseHardware;
