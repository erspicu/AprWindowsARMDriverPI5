/*++
Module Name: minwave.h
Abstract:    CMiniportWaveRT - IMiniportWaveRT for the RP1 I2S render endpoint.
--*/
#pragma once
#include "common.h"

class CMiniportWaveRT : public IMiniportWaveRT, public CUnknown
{
public:
    DECLARE_STD_UNKNOWN();
    CMiniportWaveRT(_In_opt_ PUNKNOWN OuterUnknown)
        : CUnknown(OuterUnknown), m_Port(NULL) {}
    ~CMiniportWaveRT();

    // GetDescription, DataRangeIntersection, Init, NewStream, GetDeviceDescription
    IMP_IMiniportWaveRT;

private:
    PPORTWAVERT m_Port;
};
