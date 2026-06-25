/*++
Module Name: isp.c
Abstract:    RAW10 unpack + Bayer classification. See isp.h.
--*/
#include "isp.h"

ULONG
Raw10Unpack(_In_reads_(PixelGroups * 5) const UCHAR *Packed,
            _Out_writes_(PixelGroups * 4) USHORT *Out, _In_ ULONG PixelGroups)
{
    ULONG g, o = 0;
    for (g = 0; g < PixelGroups; g++) {
        const UCHAR *b = Packed + g * 5;
        UCHAR lsb = b[4];
        Out[o++] = (USHORT)(((USHORT)b[0] << 2) | ((lsb >> 0) & 0x3u));
        Out[o++] = (USHORT)(((USHORT)b[1] << 2) | ((lsb >> 2) & 0x3u));
        Out[o++] = (USHORT)(((USHORT)b[2] << 2) | ((lsb >> 4) & 0x3u));
        Out[o++] = (USHORT)(((USHORT)b[3] << 2) | ((lsb >> 6) & 0x3u));
    }
    return o;
}

BAYER_COLOR
BayerColorRGGB(_In_ ULONG X, _In_ ULONG Y)
{
    if ((Y & 1u) == 0) {
        return (X & 1u) == 0 ? BAYER_R : BAYER_G;   /* even row: R G R G */
    }
    return (X & 1u) == 0 ? BAYER_G : BAYER_B;        /* odd row:  G B G B */
}
