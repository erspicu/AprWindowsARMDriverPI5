/*++
Module Name: common.h
Abstract:    RP1 clock controller - KMDF driver. Maps the clockman MMIO; PLL
             configure + clock gating via clk_hw.c. Many RP1 peripherals depend
             on this for their reference clocks.
--*/
#pragma once
#include <ntddk.h>
#include <wdf.h>
#include "clk_hw.h"

#define RP1CLK_TAG  'kClR'   /* 'RlCk' */

typedef struct _RP1CLK_CONTEXT {
    PVOID  Base;
    SIZE_T Length;
} RP1CLK_CONTEXT, *PRP1CLK_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(RP1CLK_CONTEXT, Rp1ClkGetContext)

EVT_WDF_DRIVER_DEVICE_ADD       Rp1ClkEvtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE Rp1ClkEvtPrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE Rp1ClkEvtReleaseHardware;
