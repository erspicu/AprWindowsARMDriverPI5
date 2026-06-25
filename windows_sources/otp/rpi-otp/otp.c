/*++
Module Name: otp.c
Abstract:    Raspberry Pi OTP mailbox message build/parse. See otp.h.
--*/
#include "otp.h"

ULONG
OtpBuildReadMsg(_Out_writes_(CapWords) ULONG *Buf, _In_ ULONG CapWords,
                _In_ ULONG Tag, _In_ ULONG Block, _In_ ULONG StartRow,
                _In_ ULONG NumWords)
{
    ULONG valWords = 3u + NumWords;          /* block, startRow, numWords + data */
    ULONG total    = 2u + 3u + valWords + 1u;/* hdr + tag-hdr + value + end */
    ULONG i, pos;

    if (total > CapWords) {
        return 0;
    }
    Buf[1] = 0;                               /* request code */
    Buf[2] = Tag;
    Buf[3] = valWords * 4u;                   /* value buffer size in bytes */
    Buf[4] = 0;                               /* req/resp code */
    Buf[5] = Block;
    Buf[6] = StartRow;
    Buf[7] = NumWords;
    pos = 8;
    for (i = 0; i < NumWords; i++) {
        Buf[pos++] = 0;                       /* row data (filled by firmware on read) */
    }
    Buf[pos++] = 0;                           /* end tag */
    Buf[0] = pos * 4u;                        /* total size in bytes */
    return Buf[0];
}

ULONG
OtpParseResponse(_In_ const ULONG *Buf, _Out_writes_(MaxRows) ULONG *Rows, _In_ ULONG MaxRows)
{
    ULONG numWords = Buf[7];                  /* echoed back by the firmware */
    ULONG i;

    if (numWords > MaxRows) {
        numWords = MaxRows;
    }
    for (i = 0; i < numWords; i++) {
        Rows[i] = Buf[8 + i];                 /* row data starts after block/index/words */
    }
    return numWords;
}
