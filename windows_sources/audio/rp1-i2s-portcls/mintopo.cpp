/*++
Module Name: mintopo.cpp
Abstract:    CMiniportTopology implementation (minimal output topology).
--*/
#include "mintopo.h"

CMiniportTopology::~CMiniportTopology()
{
    if (m_Port) {
        m_Port->Release();
        m_Port = NULL;
    }
}

STDMETHODIMP_(NTSTATUS)
CMiniportTopology::NonDelegatingQueryInterface(_In_ REFIID Interface, _COM_Outptr_ PVOID * Object)
{
    PAGED_CODE();
    if (IsEqualGUIDAligned(Interface, IID_IUnknown)) {
        *Object = (PVOID)(PUNKNOWN)(PMINIPORTTOPOLOGY)this;
    } else if (IsEqualGUIDAligned(Interface, IID_IMiniport)) {
        *Object = (PVOID)(PMINIPORT)this;
    } else if (IsEqualGUIDAligned(Interface, IID_IMiniportTopology)) {
        *Object = (PVOID)(PMINIPORTTOPOLOGY)this;
    } else {
        *Object = NULL;
    }
    if (*Object) {
        ((PUNKNOWN)*Object)->AddRef();
        return STATUS_SUCCESS;
    }
    return STATUS_INVALID_PARAMETER;
}

STDMETHODIMP_(NTSTATUS)
CMiniportTopology::GetDescription(_Out_ PPCFILTER_DESCRIPTOR * Description)
{
    PAGED_CODE();
    *Description = &g_TopoFilterDescriptor;
    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS)
CMiniportTopology::DataRangeIntersection(_In_ ULONG PinId, _In_ PKSDATARANGE DataRange,
    _In_ PKSDATARANGE MatchingDataRange, _In_ ULONG OutputBufferLength,
    _Out_writes_bytes_to_opt_(OutputBufferLength, *ResultantFormatLength) PVOID ResultantFormat,
    _Out_ PULONG ResultantFormatLength)
{
    UNREFERENCED_PARAMETER(PinId);
    UNREFERENCED_PARAMETER(DataRange);
    UNREFERENCED_PARAMETER(MatchingDataRange);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(ResultantFormat);
    *ResultantFormatLength = 0;
    return STATUS_NOT_IMPLEMENTED;
}

STDMETHODIMP_(NTSTATUS)
CMiniportTopology::Init(_In_ PUNKNOWN UnknownAdapter, _In_ PRESOURCELIST ResourceList, _In_ PPORTTOPOLOGY Port)
{
    UNREFERENCED_PARAMETER(UnknownAdapter);
    UNREFERENCED_PARAMETER(ResourceList);
    PAGED_CODE();
    m_Port = Port;
    m_Port->AddRef();
    return STATUS_SUCCESS;
}

// ---- factory ----
NTSTATUS CreateRp1TopoMiniport(_Out_ PUNKNOWN * OutUnknown, _In_opt_ PUNKNOWN OuterUnknown)
{
    PAGED_CODE();
    CMiniportTopology * obj = new(NonPagedPoolNx, RP1AUD_POOLTAG) CMiniportTopology(OuterUnknown);
    if (obj == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    *OutUnknown = (PUNKNOWN)(PMINIPORTTOPOLOGY)obj;
    (*OutUnknown)->AddRef();
    return STATUS_SUCCESS;
}
