/*++
Module Name: common.h
Abstract:    RP1/vc4 Display-Only Driver (WDDM DOD) - Phase: GPU/Display Stage A.
             Brings up an HDMI framebuffer via dxgkrnl (display-only, no 3D).
             Hardware modeset (vc4 HVS/PixelValve/HDMI) is Stage B.
--*/
#pragma once

#include <ntddk.h>
#include <dispmprt.h>     // pulls d3dkmddi.h / d3dkmdt.h; KMDDOD_INITIALIZATION_DATA

#define RP1DOD_POOLTAG  '1DOD'   // 'DOD1'

// Per-adapter context (filled out in Stage B).
typedef struct _RP1DOD_DEVICE {
    PVOID             DxgkHandle;      // handle from DxgkDdiStartDevice
    DXGKRNL_INTERFACE DxgkInterface;   // callbacks into dxgkrnl
    DXGK_DEVICE_INFO  DeviceInfo;
    PHYSICAL_ADDRESS FbPhysical;       // HDMI framebuffer base (Stage B)
    PVOID            FbVirtual;
    ULONG            FbLength;
    ULONG            Width, Height, Pitch;
} RP1DOD_DEVICE, *PRP1DOD_DEVICE;
