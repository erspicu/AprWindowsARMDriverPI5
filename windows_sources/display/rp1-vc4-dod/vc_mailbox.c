/*++
Module Name: vc_mailbox.c
Abstract:    VideoCore mailbox property-message builder. See vc_mailbox.h.
--*/
#include "vc_mailbox.h"

void
VcMboxInit(_Out_ VC_MBOX *M, _Out_writes_(CapWords) ULONG *Buf, _In_ ULONG CapWords)
{
    M->Buf   = Buf;
    M->Cap   = CapWords;
    M->Pos   = 2;          /* [0] total size, [1] request code */
    M->Error = 0;
    if (CapWords < 3) {    /* need at least size + reqcode + end tag */
        M->Error = 1;
        return;
    }
    Buf[1] = 0;            /* request */
}

int
VcMboxAddTag(_Inout_ VC_MBOX *M, _In_ ULONG TagId, _In_ ULONG ValBufSize,
             _In_reads_opt_(ValWords) const ULONG *Val, _In_ ULONG ValWords)
{
    ULONG valWordsCap = (ValBufSize + 3u) / 4u;   /* value space, rounded up */
    ULONG need = 3u + valWordsCap;                /* tagid + size + reqresp + value */
    ULONG i;

    if (M->Error) {
        return -1;
    }
    /* +1 reserves room for the end tag that Finalize will write */
    if (M->Pos + need + 1u > M->Cap) {
        M->Error = 1;
        return -1;
    }
    if (ValWords > valWordsCap) {
        M->Error = 1;
        return -1;
    }
    M->Buf[M->Pos++] = TagId;
    M->Buf[M->Pos++] = ValBufSize;
    M->Buf[M->Pos++] = 0;                          /* req/resp code */
    for (i = 0; i < valWordsCap; i++) {
        M->Buf[M->Pos++] = (i < ValWords && Val != 0) ? Val[i] : 0u;
    }
    return 0;
}

ULONG
VcMboxFinalize(_Inout_ VC_MBOX *M)
{
    ULONG totalBytes;
    if (M->Error || M->Pos + 1u > M->Cap) {
        return 0;
    }
    M->Buf[M->Pos++] = 0;                 /* end tag */
    totalBytes = M->Pos * 4u;
    M->Buf[0] = totalBytes;               /* total size in bytes */
    return totalBytes;
}

ULONG
VcMboxSetPhysSize(_Out_writes_(CapWords) ULONG *Buf, _In_ ULONG CapWords,
                  _In_ ULONG Width, _In_ ULONG Height)
{
    VC_MBOX m;
    ULONG wh[2];
    wh[0] = Width;
    wh[1] = Height;
    VcMboxInit(&m, Buf, CapWords);
    VcMboxAddTag(&m, VCTAG_SET_PHYS_WH, 8, wh, 2);
    return VcMboxFinalize(&m);
}

ULONG
VcMboxGetEdidBlock(_Out_writes_(CapWords) ULONG *Buf, _In_ ULONG CapWords,
                   _In_ ULONG Block)
{
    VC_MBOX m;
    ULONG blk = Block;
    VcMboxInit(&m, Buf, CapWords);
    /* value buffer: block# (4) + status (4) + 128B EDID = 136 bytes */
    VcMboxAddTag(&m, VCTAG_GET_EDID_BLOCK, 136, &blk, 1);
    return VcMboxFinalize(&m);
}
