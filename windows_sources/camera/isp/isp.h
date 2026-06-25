/*++
Module Name: isp.h
Abstract:    Camera front-end pixel logic for the DeviceMFT software ISP
             (see MD/Note/camera/): MIPI CSI-2 RAW10 unpack (4 px per 5 bytes)
             and Bayer color classification. IMX708 default is SRGGB10 (RGGB).
             OS-independent pure logic; x64-sim verified (ISP_SIM).
--*/
#pragma once

#ifdef ISP_SIM
#include "sim/isp_simshim.h"
#else
#include <ntddk.h>
#endif

typedef enum { BAYER_R = 0, BAYER_G = 1, BAYER_B = 2 } BAYER_COLOR;

/* MIPI RAW10: bytes b0..b3 = MSBs of px0..px3, b4 = packed LSBs (2 bits each).
   Unpack PixelGroups groups of 4 (PixelGroups*5 packed bytes) into Out (10-bit
   values, PixelGroups*4 entries). Returns pixels written. */
ULONG Raw10Unpack(_In_reads_(PixelGroups * 5) const UCHAR *Packed,
                  _Out_writes_(PixelGroups * 4) USHORT *Out, _In_ ULONG PixelGroups);

/* Bayer color at (x,y) for an RGGB sensor (IMX708): R G / G B repeating. */
BAYER_COLOR BayerColorRGGB(_In_ ULONG X, _In_ ULONG Y);
