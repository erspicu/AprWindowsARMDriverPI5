/*++
Module Name: edid.c
Abstract:    EDID base-block parser. See edid.h.
--*/
#include "edid.h"

int
EdidParse(_In_reads_(Len) const UCHAR *Edid, _In_ ULONG Len, _Out_ EDID_INFO *Out)
{
    static const UCHAR header[8] = { 0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00 };
    ULONG i, sum = 0;
    const UCHAR *d;
    ULONG hActive, vActive, pixClk10kHz;

    Out->Valid = 0;
    Out->Width = Out->Height = 0;
    Out->PixelClockKHz = 0;

    if (Edid == 0 || Len < EDID_BLOCK_SIZE) {
        return -1;
    }
    for (i = 0; i < 8; i++) {
        if (Edid[i] != header[i]) {
            return -1;                 /* not an EDID base block */
        }
    }
    for (i = 0; i < EDID_BLOCK_SIZE; i++) {
        sum += Edid[i];
    }
    if ((sum & 0xFFu) != 0) {
        return -1;                     /* checksum failure */
    }

    /* first Detailed Timing Descriptor (preferred mode) */
    d = Edid + EDID_DTD_OFFSET;
    pixClk10kHz = (ULONG)d[0] | ((ULONG)d[1] << 8);
    if (pixClk10kHz == 0) {
        return -1;                     /* first descriptor is not a DTD */
    }
    hActive = (ULONG)d[2] | (((ULONG)d[4] & 0xF0u) << 4);
    vActive = (ULONG)d[5] | (((ULONG)d[7] & 0xF0u) << 4);

    Out->Width         = (USHORT)hActive;
    Out->Height        = (USHORT)vActive;
    Out->PixelClockKHz = pixClk10kHz * 10u;
    Out->Valid         = 1;
    return 0;
}
