/*++
Module Name: common.h
Abstract:    RP1 CFE (Camera Front End) + PiSP ISP - AVStream minidriver skeleton.
             Registers an AVStream (ks.sys) minidriver so the camera surfaces as
             a KS capture device. This skeleton stands up the device descriptor;
             the capture KSFILTERFACTORY (sensor MIPI-CSI2 -> ISP -> video pin),
             buffer/DMA path and sensor I2C control are the next (large) stages.
--*/
#pragma once

#include <ntddk.h>
/* ks.h references a few Win32 types/macros (BOOL, WINAPI) in some prototypes;
   kernel headers do not define them, so provide them before including ks.h. */
typedef int BOOL;
#ifndef WINAPI
#define WINAPI __stdcall
#endif
#include <ks.h>

#define CFE_TAG  'efcR'   // 'Rcfe'

/* AVStream device dispatch callback */
NTSTATUS CfeDeviceAdd(_In_ PKSDEVICE Device);
