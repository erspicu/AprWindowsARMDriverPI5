/*++

Module Name:
    common.h

Abstract:
    RP1 I2S PortCls/WaveRT audio driver - Phase 2 (in progress).
    Shared includes, pool tag, descriptor externs, miniport factory decls.

    Audio model: PortCls + WaveRT (the correct Windows audio architecture, and
    a WDM-based stack - which is why Phase 1 used WDM as the foundation).
    The hardware layer is reused from ..\rp1-i2s\rp1_i2s_hw.c (ported I2S HAL).

--*/

#pragma once

// INITGUID must precede the GUID-defining headers so that the KS / PortCls
// GUIDs (KSDATAFORMAT_TYPE_AUDIO, CLSID_PortWaveRT, ...) are emitted as
// __declspec(selectany) definitions in every TU and merged by the linker.
#define INITGUID
#include <initguid.h>
#include <portcls.h>
#include <ksmedia.h>
#include <stdunk.h>

#define RP1AUD_POOLTAG   'A2IR'    // 'RI2A'

#define RP1_ARRAYCOUNT(a)  (sizeof(a) / sizeof((a)[0]))

// Filter descriptor tables (tables.cpp)
extern PCFILTER_DESCRIPTOR g_WaveFilterDescriptor;
extern PCFILTER_DESCRIPTOR g_TopoFilterDescriptor;

// Miniport factory functions (minwave.cpp / mintopo.cpp)
NTSTATUS CreateRp1WaveRTMiniport(_Out_ PUNKNOWN * OutUnknown, _In_opt_ PUNKNOWN OuterUnknown);
NTSTATUS CreateRp1TopoMiniport  (_Out_ PUNKNOWN * OutUnknown, _In_opt_ PUNKNOWN OuterUnknown);
