/*++
Module Name: sdio_core.c
Abstract:    CYW43455 SDIO control-plane logic + WHD NVRAM preprocessing.
             See sdio_core.h. Pure logic, x64-sim verified (CYW_SIM).
--*/
#include "sdio_core.h"

int
SdioSetBackplaneWindow(_In_ const SDIO_OPS *Ops, _In_ ULONG Addr)
{
    ULONG window = Addr & ~(ULONG)SBSDIO_SB_OFT_ADDR_MASK;  /* clear low 15 bits */
    UCHAR lo  = (UCHAR)((window >> 8)  & 0xFF);
    UCHAR mid = (UCHAR)((window >> 16) & 0xFF);
    UCHAR hi  = (UCHAR)((window >> 24) & 0xFF);
    int rc;

    rc = Ops->Cmd52(Ops->Ctx, SDIO_FUNC_BACKPLANE, SBSDIO_FUNC1_SBADDRLOW,  &lo,  1);
    if (rc) return rc;
    rc = Ops->Cmd52(Ops->Ctx, SDIO_FUNC_BACKPLANE, SBSDIO_FUNC1_SBADDRMID,  &mid, 1);
    if (rc) return rc;
    rc = Ops->Cmd52(Ops->Ctx, SDIO_FUNC_BACKPLANE, SBSDIO_FUNC1_SBADDRHIGH, &hi,  1);
    return rc;
}

int
SdioBackplaneRead32(_In_ const SDIO_OPS *Ops, _In_ ULONG Addr, _Out_ ULONG *Value)
{
    UCHAR buf[4] = { 0, 0, 0, 0 };
    ULONG sdioAddr;
    int rc;

    rc = SdioSetBackplaneWindow(Ops, Addr);
    if (rc) return rc;

    /* the 15-bit offset within the window is read through the F1 window region */
    sdioAddr = SBSDIO_SB_ACCESS_WINDOW | (Addr & SBSDIO_SB_OFT_ADDR_MASK);

    rc = Ops->Cmd53(Ops->Ctx, SDIO_FUNC_BACKPLANE, sdioAddr, buf, 4, 0 /*read*/);
    if (rc) return rc;

    *Value = (ULONG)buf[0] | ((ULONG)buf[1] << 8) |
             ((ULONG)buf[2] << 16) | ((ULONG)buf[3] << 24);
    return 0;
}

int
Cyw43455ReadChipId(_In_ const SDIO_OPS *Ops, _Out_ USHORT *ChipId)
{
    ULONG reg = 0;
    int rc = SdioBackplaneRead32(Ops, CYW43455_CHIPCOMMON_BASE, &reg);
    if (rc) return rc;
    *ChipId = (USHORT)(reg & 0xFFFF);   /* chip id is the low 16 bits */
    return 0;
}

ULONG
WhdNvramPreprocess(_In_reads_(InLen) const char *In, _In_ ULONG InLen,
                   _Out_writes_(OutCap) PUCHAR Out, _In_ ULONG OutCap)
{
    ULONG i = 0, o = 0;

    while (i < InLen) {
        ULONG ls = i, le;
        const char *p;
        ULONG llen, k;

        /* find end of this line (exclusive of '\n') */
        while (i < InLen && In[i] != '\n') {
            i++;
        }
        le = i;                 /* [ls, le) is the raw line */
        if (i < InLen) {
            i++;                /* consume the '\n' */
        }

        /* trim a trailing '\r' (CRLF input) */
        if (le > ls && In[le - 1] == '\r') {
            le--;
        }

        /* skip leading spaces/tabs to classify the line */
        p = In + ls;
        while ((ULONG)(p - (In + ls)) < (le - ls) && (*p == ' ' || *p == '\t')) {
            p++;
        }
        llen = (ULONG)(le - (ULONG)(p - In));

        if (llen == 0 || *p == '#') {
            continue;           /* blank or comment line -> drop */
        }

        /* copy "key=value" then a single NUL terminator */
        if (o + llen + 1 > OutCap) {
            return 0;           /* overflow */
        }
        for (k = 0; k < llen; k++) {
            Out[o++] = (UCHAR)p[k];
        }
        Out[o++] = 0x00;
    }

    /* whole pool ends with an extra NUL (so the tail is "\0\0") */
    if (o + 1 > OutCap) {
        return 0;
    }
    Out[o++] = 0x00;
    return o;
}

ULONG
WhdResourceGetBlock(_In_reads_(Total) const UCHAR *Data, _In_ ULONG Total,
                    _In_ ULONG BlockNo, _In_ ULONG BlockSize,
                    _Out_ const UCHAR **OutPtr)
{
    ULONG offset, remain;

    *OutPtr = (const UCHAR *)0;
    if (BlockSize == 0) {
        return 0;
    }
    offset = BlockNo * BlockSize;
    if (offset >= Total) {
        return 0;                 /* past the end - WHD stops requesting */
    }
    remain = Total - offset;
    *OutPtr = Data + offset;
    return (remain < BlockSize) ? remain : BlockSize;
}
