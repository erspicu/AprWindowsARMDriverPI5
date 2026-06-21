/*++
Module Name: mintopo.h
Abstract:    CMiniportTopology - IMiniportTopology (DAC -> speaker endpoint).
--*/
#pragma once
#include "common.h"

class CMiniportTopology : public IMiniportTopology, public CUnknown
{
public:
    DECLARE_STD_UNKNOWN();
    CMiniportTopology(_In_opt_ PUNKNOWN OuterUnknown)
        : CUnknown(OuterUnknown), m_Port(NULL) {}
    ~CMiniportTopology();

    // GetDescription, DataRangeIntersection, Init
    IMP_IMiniportTopology;

private:
    PPORTTOPOLOGY m_Port;
};
