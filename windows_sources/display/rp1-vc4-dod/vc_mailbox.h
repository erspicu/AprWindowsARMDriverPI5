/*++
Module Name: vc_mailbox.h
Abstract:    VideoCore mailbox PROPERTY-channel message builder for the DOD's
             hybrid modeset (let the VideoCore firmware do HDMI modeset + buffer
             alloc + EDID read; the driver only flips). Builds the u32 tag buffer:
               [total_size_bytes][req_code=0][ tag ... ][end_tag=0]
             each tag = [tag_id][value_buf_size][req/resp=0][ value words... ].
             OS-independent pure logic; x64-sim verified (DISP_SIM).
--*/
#pragma once

#ifdef DISP_SIM
#include "sim/disp_simshim.h"
#else
#include <ntddk.h>
#endif

/* VideoCore property tags used by the DOD */
#define VCTAG_ALLOC_BUFFER     0x00040001u   /* in: alignment; out: base,size   */
#define VCTAG_SET_PHYS_WH      0x00048003u   /* in/out: width, height           */
#define VCTAG_SET_VIRT_WH      0x00048004u
#define VCTAG_SET_DEPTH        0x00048005u   /* in/out: bits per pixel          */
#define VCTAG_GET_EDID_BLOCK   0x00030020u   /* in: block#; out: block#,status,128B */

typedef struct _VC_MBOX {
    ULONG *Buf;       /* u32 message buffer */
    ULONG  Cap;       /* capacity in u32 words */
    ULONG  Pos;       /* next write index (words) */
    int    Error;
} VC_MBOX;

/* begin a message in Buf (cap words). Reserves [0]=size, [1]=request-code(0). */
void  VcMboxInit(_Out_ VC_MBOX *M, _Out_writes_(CapWords) ULONG *Buf, _In_ ULONG CapWords);

/* append one tag: ValBufSize bytes of value space, of which ValWords u32s are
   copied from Val (input), the rest zero-filled (room for the response). */
int   VcMboxAddTag(_Inout_ VC_MBOX *M, _In_ ULONG TagId, _In_ ULONG ValBufSize,
                   _In_reads_opt_(ValWords) const ULONG *Val, _In_ ULONG ValWords);

/* write the end tag + total size; returns total message size in bytes (0 on error) */
ULONG VcMboxFinalize(_Inout_ VC_MBOX *M);

/* convenience one-tag messages */
ULONG VcMboxSetPhysSize(_Out_writes_(CapWords) ULONG *Buf, _In_ ULONG CapWords,
                        _In_ ULONG Width, _In_ ULONG Height);
ULONG VcMboxGetEdidBlock(_Out_writes_(CapWords) ULONG *Buf, _In_ ULONG CapWords,
                         _In_ ULONG Block);
