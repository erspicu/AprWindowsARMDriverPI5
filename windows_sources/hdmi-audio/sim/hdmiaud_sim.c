/*++
Module Name: hdmiaud_sim.c
Abstract:    x64 simulation of the HDMI audio logic (hdmi_audio.c): ACR N/CTS and
             the CEA-861 Audio InfoFrame. Build: cl /DHDMIAUD_SIM /I.. hdmiaud_sim.c ..\hdmi_audio.c
--*/
#include <stdio.h>
#include "../hdmi_audio.h"

static int g_pass, g_fail;
static void check(const char *what, int cond)
{
    if (cond) { g_pass++; printf("  [PASS] %s\n", what); }
    else      { g_fail++; printf("  [FAIL] %s\n", what); }
}

int main(void)
{
    ULONG n, cts, i, sum;
    UCHAR ifr[HDMI_AUDIO_INFOFRAME_LEN];

    printf("== HDMI audio logic simulation ==\n");

    printf("-- ACR N/CTS --\n");
    /* 1080p60: pixel clock 148.5 MHz, 48 kHz -> N=6144, CTS=148500 */
    HdmiAcrComputeNCts(148500000u, 48000u, &n, &cts);
    check("48k N == 6144", n == 6144);
    check("48k @148.5MHz CTS == 148500", cts == 148500);
    /* 32 kHz -> N=4096 */
    HdmiAcrComputeNCts(148500000u, 32000u, &n, &cts);
    check("32k N == 4096", n == 4096);
    /* 2560x1440@60 ~241.5 MHz, 48k -> CTS = 241500*6144/6144 = 241500 */
    HdmiAcrComputeNCts(241500000u, 48000u, &n, &cts);
    check("48k @241.5MHz CTS == 241500", cts == 241500);

    printf("-- Audio InfoFrame --\n");
    /* 2-channel, 48 kHz, 16-bit */
    check("infoframe build ok", HdmiAudioInfoFrame(ifr, 2, HDMI_SF_48K, HDMI_SS_16BIT) == 0);
    check("type == 0x84", ifr[0] == 0x84);
    check("version == 1", ifr[1] == 0x01);
    check("length == 10", ifr[2] == 0x0A);
    check("PB1 CC == 1 (2ch-1)", (ifr[4] & 0x7) == 1);
    check("PB2 SF==48k SS==16", ((ifr[5] >> 2) & 0x7) == HDMI_SF_48K && (ifr[5] & 0x3) == HDMI_SS_16BIT);
    sum = 0;
    for (i = 0; i < HDMI_AUDIO_INFOFRAME_LEN; i++) sum += ifr[i];
    check("checksum makes byte-sum 0 mod 256", (sum & 0xFF) == 0);
    /* invalid channel count rejected */
    check("9 channels rejected", HdmiAudioInfoFrame(ifr, 9, HDMI_SF_48K, HDMI_SS_16BIT) != 0);

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
