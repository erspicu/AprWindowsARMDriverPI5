/*++

Module Name:
    tables.cpp

Abstract:
    PortCls filter descriptor tables for the RP1 I2S render endpoint.

    Wave filter:   pin0 = host sink (IN), pin1 = bridge (OUT) -> topology.
    Topology filter: pin0 = bridge (IN) -> DAC node -> pin1 = speaker (OUT).
    Minimal single render path: PCM 48 kHz / 16-bit / stereo.

--*/

#include "common.h"

// ---- Render data range: PCM, 48 kHz, 16-bit, stereo ----
static KSDATARANGE_AUDIO PinDataRangesRender[] =
{
    {
        {
            sizeof(KSDATARANGE_AUDIO), 0, 0, 0,
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        2,        // MaximumChannels
        16,       // MinimumBitsPerSample
        16,       // MaximumBitsPerSample
        48000,    // MinimumSampleFrequency
        48000     // MaximumSampleFrequency
    }
};

static PKSDATARANGE PinDataRangePointersRender[] =
{
    (PKSDATARANGE)&PinDataRangesRender[0]
};

// ============================ WAVE FILTER ============================
static PCPIN_DESCRIPTOR WavePins[] =
{
    // pin 0: host sink (render data in from the audio engine)
    {
        1, 1, 0, NULL,
        {
            0, NULL,
            0, NULL,
            RP1_ARRAYCOUNT(PinDataRangePointersRender), PinDataRangePointersRender,
            KSPIN_DATAFLOW_IN,
            KSPIN_COMMUNICATION_SINK,
            &KSCATEGORY_AUDIO,
            NULL
        }
    },
    // pin 1: bridge out to the topology filter
    {
        0, 0, 0, NULL,
        {
            0, NULL,
            0, NULL,
            RP1_ARRAYCOUNT(PinDataRangePointersRender), PinDataRangePointersRender,
            KSPIN_DATAFLOW_OUT,
            KSPIN_COMMUNICATION_NONE,
            &KSCATEGORY_AUDIO,
            NULL
        }
    }
};

PCFILTER_DESCRIPTOR g_WaveFilterDescriptor =
{
    0,                                      // Version
    NULL,                                   // AutomationTable
    sizeof(PCPIN_DESCRIPTOR),               // PinSize
    RP1_ARRAYCOUNT(WavePins),               // PinCount
    WavePins,                               // Pins
    0,                                      // NodeSize
    0,                                      // NodeCount
    NULL,                                   // Nodes
    0,                                      // ConnectionCount
    NULL,                                   // Connections
    0,                                      // CategoryCount
    NULL                                    // Categories
};

// ========================== TOPOLOGY FILTER =========================
static PCPIN_DESCRIPTOR TopoPins[] =
{
    // pin 0: bridge in (from wave filter)
    {
        0, 0, 0, NULL,
        {
            0, NULL, 0, NULL, 0, NULL,
            KSPIN_DATAFLOW_IN,
            KSPIN_COMMUNICATION_NONE,
            &KSCATEGORY_AUDIO,
            NULL
        }
    },
    // pin 1: physical speaker output
    {
        0, 0, 0, NULL,
        {
            0, NULL, 0, NULL, 0, NULL,
            KSPIN_DATAFLOW_OUT,
            KSPIN_COMMUNICATION_NONE,
            &KSNODETYPE_SPEAKER,
            NULL
        }
    }
};

static PCNODE_DESCRIPTOR TopoNodes[] =
{
    { 0, NULL, &KSNODETYPE_DAC, NULL }      // node 0: DAC
};

static PCCONNECTION_DESCRIPTOR TopoConnections[] =
{
    { PCFILTER_NODE, 0, 0,            1 },  // filter pin0 (IN)  -> DAC node pin-in
    { 0,            0, PCFILTER_NODE, 1 }   // DAC node pin-out  -> filter pin1 (OUT)
};

PCFILTER_DESCRIPTOR g_TopoFilterDescriptor =
{
    0,                                      // Version
    NULL,                                   // AutomationTable
    sizeof(PCPIN_DESCRIPTOR),               // PinSize
    RP1_ARRAYCOUNT(TopoPins),               // PinCount
    TopoPins,                               // Pins
    sizeof(PCNODE_DESCRIPTOR),              // NodeSize
    RP1_ARRAYCOUNT(TopoNodes),              // NodeCount
    TopoNodes,                              // Nodes
    RP1_ARRAYCOUNT(TopoConnections),        // ConnectionCount
    TopoConnections,                        // Connections
    0,                                      // CategoryCount
    NULL                                    // Categories
};
