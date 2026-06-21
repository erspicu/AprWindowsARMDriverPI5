/*++
Module Name: common.h
Abstract:    RP1 PWM (fan/LED PWM) - KMDF function driver. Maps the PWM MMIO and
             drives channels via pwm_hw.c. (Windows has no PWM device class, so
             control is exposed via the driver's own IOCTLs - left as TODO.)
--*/
#pragma once
#include <ntddk.h>
#include <wdf.h>
#include "pwm_hw.h"

#define RP1PWM_TAG  'wPpR'   /* 'RpPw' */

typedef struct _RP1PWM_CONTEXT {
    PVOID  Base;
    SIZE_T Length;
} RP1PWM_CONTEXT, *PRP1PWM_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(RP1PWM_CONTEXT, Rp1PwmGetContext)

EVT_WDF_DRIVER_DEVICE_ADD       Rp1PwmEvtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE Rp1PwmEvtPrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE Rp1PwmEvtReleaseHardware;
