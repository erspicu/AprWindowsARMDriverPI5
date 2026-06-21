/*++
Module Name: minwavestream.h
Abstract:    CMiniportWaveRTStream - IMiniportWaveRTStream for one render stream.
             Bridges PortCls to the ported DesignWare I2S HAL.
--*/
#pragma once
#include "common.h"
#include "../rp1-i2s/rp1_i2s_hw.h"   // ported HAL (C, extern "C" guarded)

class CMiniportWaveRTStream : public IMiniportWaveRTStream, public CUnknown
{
public:
    DECLARE_STD_UNKNOWN();
    CMiniportWaveRTStream(_In_opt_ PUNKNOWN OuterUnknown)
        : CUnknown(OuterUnknown), m_Port(NULL), m_BufferMdl(NULL),
          m_BufferSize(0), m_Position(0) {}
    ~CMiniportWaveRTStream();

    // SetFormat, SetState, GetPosition, AllocateAudioBuffer, FreeAudioBuffer,
    // GetHWLatency, GetPositionRegister, GetClockRegister
    IMP_IMiniportWaveRTStream;

    NTSTATUS Init(_In_ PPORTWAVERTSTREAM PortStream, _In_ ULONG Pin, _In_ PKSDATAFORMAT DataFormat);

private:
    PPORTWAVERTSTREAM m_Port;
    PMDL              m_BufferMdl;
    ULONG             m_BufferSize;
    ULONGLONG         m_Position;
    RP1I2S_HW         m_Hw;       // HAL state; Base==NULL until real RP1 MMIO wired
};
