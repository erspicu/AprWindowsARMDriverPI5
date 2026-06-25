/*++
Module Name: sand.h
Abstract:    Broadcom SAND (column-tiled) -> linear NV12 detile for the rpivid
             HEVC decoder output (see MD/Note/hevc/). rpivid writes
             V4L2_PIX_FMT_NV12_COL128: the image is stored as vertical 128-byte
             columns, each `colHeight` rows tall; columns laid left-to-right.
             A byte at luma pixel (x,y) is:
               off = (x>>7)*(colHeight*128) + y*128 + (x & 127)
             (from hevc_d_h265.c: luma_stride = height*128). The DOD/MFT must
             detile this to linear NV12 before display (the notes' key gotcha).
             OS-independent pure logic; x64-sim verified (HEVC_SIM).
--*/
#pragma once

#ifdef HEVC_SIM
#include "sim/hevc_simshim.h"
#else
#include <ntddk.h>
#endif

#define SAND_COL_WIDTH  128u

/* byte offset of luma pixel (x,y) within a SAND (COL128) plane */
ULONG SandColOffset(_In_ ULONG X, _In_ ULONG Y, _In_ ULONG ColHeightRows);

/* detile a SAND luma plane into a linear (width-stride) plane */
void SandToLinearY(_In_reads_(ColHeightRows * ((Width + 127) / 128) * 128) const UCHAR *Sand,
                   _Out_writes_(Width * Height) UCHAR *Linear,
                   _In_ ULONG Width, _In_ ULONG Height, _In_ ULONG ColHeightRows);
