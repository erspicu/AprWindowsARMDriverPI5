/*++
Module Name: hdmi_audio.h
Abstract:    HDMI audio pure logic for the PortCls/WaveRT driver (see
             MD/Note/hdmi-audio/): Audio Clock Regeneration (N/CTS) and the
             CEA-861 Audio InfoFrame. ACR formula taken from the Linux vc4_hdmi
             driver (vc4_hdmi_set_n_cts): N = 128*fs/1000, CTS = pixClk*N/(128*fs).
             OS-independent; x64-sim verified (HDMIAUD_SIM).
--*/
#pragma once

#ifdef HDMIAUD_SIM
#include "sim/hdmiaud_simshim.h"
#else
#include <ntddk.h>
#endif

/* CEA-861 audio sample-frequency codes (Audio InfoFrame SF field) */
#define HDMI_SF_REFER   0u
#define HDMI_SF_32K     1u
#define HDMI_SF_44K1    2u
#define HDMI_SF_48K     3u
#define HDMI_SF_88K2    4u
#define HDMI_SF_96K     5u
#define HDMI_SF_176K4   6u
#define HDMI_SF_192K    7u

/* sample-size codes (SS field) */
#define HDMI_SS_REFER   0u
#define HDMI_SS_16BIT   1u
#define HDMI_SS_20BIT   2u
#define HDMI_SS_24BIT   3u

#define HDMI_AUDIO_INFOFRAME_LEN  14u   /* 3 header + checksum + 10 data bytes */

/* Compute Audio Clock Regeneration N and CTS for the given pixel (TMDS) clock
   and audio sample rate (both in Hz). Matches vc4_hdmi_set_n_cts(). */
void HdmiAcrComputeNCts(_In_ ULONG PixelClockHz, _In_ ULONG SampleRateHz,
                        _Out_ ULONG *N, _Out_ ULONG *CTS);

/* Build a CEA-861 Audio InfoFrame (type 0x84) into Out (>= 14 bytes), with the
   InfoFrame checksum filled so the byte sum is 0 mod 256. Channels = 2..8. */
int HdmiAudioInfoFrame(_Out_writes_(HDMI_AUDIO_INFOFRAME_LEN) UCHAR *Out,
                       _In_ ULONG Channels, _In_ ULONG SfCode, _In_ ULONG SsCode);
