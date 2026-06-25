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

void
EdidGetDefault1080p(_Out_writes_(EDID_BLOCK_SIZE) UCHAR *Out)
{
    /* 1920x1080@60: pixclk 148.5MHz, hblank 280 (fp 88, sync 44),
       vblank 45 (fp 4, sync 5). Only the first DTD + header/version are filled;
       the rest is zero (sufficient as a fallback; our parser reads the DTD). */
    ULONG i, sum = 0;
    UCHAR *d;

    for (i = 0; i < EDID_BLOCK_SIZE; i++) {
        Out[i] = 0;
    }
    Out[0] = 0x00; Out[1] = 0xFF; Out[2] = 0xFF; Out[3] = 0xFF;
    Out[4] = 0xFF; Out[5] = 0xFF; Out[6] = 0xFF; Out[7] = 0x00;   /* header */
    Out[0x12] = 0x01; Out[0x13] = 0x04;                          /* EDID 1.4 */
    Out[0x14] = 0x80;                                            /* digital input */

    d = Out + EDID_DTD_OFFSET;            /* first Detailed Timing Descriptor */
    d[0]  = 0x02; d[1]  = 0x3A;           /* pixel clock 14850 *10kHz = 148.5MHz */
    d[2]  = 0x80;                         /* h active low (1920 & 0xFF) */
    d[3]  = 0x18;                         /* h blank low (280 & 0xFF) */
    d[4]  = 0x71;                         /* h active[11:8]=7, h blank[11:8]=1 */
    d[5]  = 0x38;                         /* v active low (1080 & 0xFF) */
    d[6]  = 0x2D;                         /* v blank low (45) */
    d[7]  = 0x40;                         /* v active[11:8]=4, v blank[11:8]=0 */
    d[8]  = 0x58;                         /* h front porch (88) */
    d[9]  = 0x2C;                         /* h sync width (44) */
    d[10] = 0x45;                         /* v front porch 4, v sync 5 */
    d[17] = 0x1E;                         /* digital separate, +h +v sync */

    for (i = 0; i < EDID_BLOCK_SIZE - 1u; i++) {
        sum += Out[i];
    }
    Out[EDID_BLOCK_SIZE - 1u] = (UCHAR)((256u - (sum & 0xFFu)) & 0xFFu);  /* checksum */
}
