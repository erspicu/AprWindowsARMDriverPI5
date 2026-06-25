/*++
Module Name: sand.c
Abstract:    Broadcom SAND (COL128) -> linear NV12 detile. See sand.h.
--*/
#include "sand.h"

ULONG
SandColOffset(_In_ ULONG X, _In_ ULONG Y, _In_ ULONG ColHeightRows)
{
    ULONG col       = X >> 7;                       /* X / 128 */
    ULONG colStride = ColHeightRows * SAND_COL_WIDTH;
    return col * colStride + Y * SAND_COL_WIDTH + (X & 127u);
}

void
SandToLinearY(_In_reads_(ColHeightRows * ((Width + 127) / 128) * 128) const UCHAR *Sand,
              _Out_writes_(Width * Height) UCHAR *Linear,
              _In_ ULONG Width, _In_ ULONG Height, _In_ ULONG ColHeightRows)
{
    ULONG colStride = ColHeightRows * SAND_COL_WIDTH;
    ULONG x, y;

    for (y = 0; y < Height; y++) {
        const UCHAR *rowBaseInCol = Sand + y * SAND_COL_WIDTH;   /* + col*colStride */
        UCHAR *dst = Linear + y * Width;
        for (x = 0; x < Width; x++) {
            ULONG col = x >> 7;
            dst[x] = rowBaseInCol[col * colStride + (x & 127u)];
        }
    }
}
