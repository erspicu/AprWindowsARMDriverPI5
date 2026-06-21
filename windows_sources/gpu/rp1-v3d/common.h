/*++
Module Name: common.h
Abstract:    RP1/V3D (VideoCore VII) WDDM full render KMD - scaffold.
             Unlike the DOD, a render KMD registers via DxgkInitialize with
             DRIVER_INITIALIZATION_DATA and must implement the render DDIs
             (CreateDevice/CreateAllocation/Patch/SubmitCommand/Render/...).
             A functional render driver also needs a user-mode D3D driver (UMD)
             and the physical V3D - so this is a buildable scaffold only.
--*/
#pragma once

#include <ntddk.h>
#include <dispmprt.h>

#define RP1V3D_POOLTAG  '3DV1'   // 'V3D1'

typedef struct _RP1V3D_ADAPTER {
    PVOID             DxgkHandle;
    DXGKRNL_INTERFACE DxgkInterface;
} RP1V3D_ADAPTER, *PRP1V3D_ADAPTER;
