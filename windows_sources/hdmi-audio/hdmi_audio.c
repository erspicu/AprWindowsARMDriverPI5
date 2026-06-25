/*++
Module Name: hdmi_audio.c
Abstract:    HDMI audio ACR (N/CTS) + Audio InfoFrame. See hdmi_audio.h.
--*/
#include "hdmi_audio.h"

void
HdmiAcrComputeNCts(_In_ ULONG PixelClockHz, _In_ ULONG SampleRateHz,
                   _Out_ ULONG *N, _Out_ ULONG *CTS)
{
    ULONG n;
    unsigned long long tmp;

    /* vc4_hdmi: n = 128 * fs / 1000 (48k->6144, 32k->4096) */
    n = (128u * SampleRateHz) / 1000u;

    /* cts = (pixelClk * n) / (128 * fs) */
    tmp = (unsigned long long)PixelClockHz * (unsigned long long)n;
    if (SampleRateHz != 0) {
        tmp /= (128ull * (unsigned long long)SampleRateHz);
    }

    *N   = n;
    *CTS = (ULONG)tmp;
}

int
HdmiAudioInfoFrame(_Out_writes_(HDMI_AUDIO_INFOFRAME_LEN) UCHAR *Out,
                   _In_ ULONG Channels, _In_ ULONG SfCode, _In_ ULONG SsCode)
{
    ULONG i, sum = 0;
    UCHAR cc;

    if (Channels < 1 || Channels > 8) {
        return -1;
    }
    for (i = 0; i < HDMI_AUDIO_INFOFRAME_LEN; i++) {
        Out[i] = 0;
    }

    /* header */
    Out[0] = 0x84;        /* InfoFrame type = Audio */
    Out[1] = 0x01;        /* version */
    Out[2] = 0x0A;        /* length = 10 data bytes */
    /* Out[3] = checksum (filled below) */

    /* PB1 (Out[4]): CT[7:4]=0 (refer to stream), CC[2:0] = channel count - 1 */
    cc = (UCHAR)((Channels - 1) & 0x7u);
    Out[4] = cc;
    /* PB2 (Out[5]): SF[4:2], SS[1:0] */
    Out[5] = (UCHAR)(((SfCode & 0x7u) << 2) | (SsCode & 0x3u));
    /* PB3 (Out[6]) = 0; PB4 (Out[7]) = CA (channel allocation), 0 = FL/FR */
    /* PB5..PB10 (Out[8..13]) = 0 */

    /* InfoFrame checksum: sum of all 14 bytes must be 0 (mod 256) */
    for (i = 0; i < HDMI_AUDIO_INFOFRAME_LEN; i++) {
        sum += Out[i];
    }
    Out[3] = (UCHAR)((256u - (sum & 0xFFu)) & 0xFFu);
    return 0;
}
