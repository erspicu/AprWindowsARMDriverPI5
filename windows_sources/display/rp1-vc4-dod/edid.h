/*++
Module Name: edid.h
Abstract:    Minimal EDID (E-DID 1.x base block) parser for the DOD: validate the
             128-byte block and extract the preferred mode (first Detailed Timing
             Descriptor) — active resolution + pixel clock. Used to answer
             DxgkDdiQueryDeviceDescriptor / build the VidPN mode list.
             OS-independent pure logic; x64-sim verified (DISP_SIM).
--*/
#pragma once

#ifdef DISP_SIM
#include "sim/disp_simshim.h"
#else
#include <ntddk.h>
#endif

#define EDID_BLOCK_SIZE   128u
#define EDID_DTD_OFFSET   54u     /* first Detailed Timing Descriptor */

typedef struct _EDID_INFO {
    USHORT Width;          /* preferred active horizontal pixels */
    USHORT Height;         /* preferred active vertical lines */
    ULONG  PixelClockKHz;  /* pixel clock in kHz */
    int    Valid;
} EDID_INFO;

/* parse the first 128-byte EDID block. Returns 0 on success (Out filled),
   non-zero if header/checksum/length is bad or the first DTD is not a timing. */
int EdidParse(_In_reads_(Len) const UCHAR *Edid, _In_ ULONG Len, _Out_ EDID_INFO *Out);

/* B3: write a minimal valid 1920x1080@60 EDID base block (148.5 MHz pixel clock)
   into Out, used as the fallback when the real monitor EDID cannot be read. */
void EdidGetDefault1080p(_Out_writes_(EDID_BLOCK_SIZE) UCHAR *Out);
