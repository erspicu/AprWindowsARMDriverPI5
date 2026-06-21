/*++
Module Name: minwave.cpp
Abstract:    CMiniportWaveRT implementation (render endpoint, minimal).
--*/
#include "minwave.h"
#include "minwavestream.h"

CMiniportWaveRT::~CMiniportWaveRT()
{
    if (m_Port) {
        m_Port->Release();
        m_Port = NULL;
    }
}

STDMETHODIMP_(NTSTATUS)
CMiniportWaveRT::NonDelegatingQueryInterface(_In_ REFIID Interface, _COM_Outptr_ PVOID * Object)
{
    PAGED_CODE();
    if (IsEqualGUIDAligned(Interface, IID_IUnknown)) {
        *Object = (PVOID)(PUNKNOWN)(PMINIPORTWAVERT)this;
    } else if (IsEqualGUIDAligned(Interface, IID_IMiniport)) {
        *Object = (PVOID)(PMINIPORT)this;
    } else if (IsEqualGUIDAligned(Interface, IID_IMiniportWaveRT)) {
        *Object = (PVOID)(PMINIPORTWAVERT)this;
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
CMiniportWaveRT::GetDescription(_Out_ PPCFILTER_DESCRIPTOR * Description)
{
    PAGED_CODE();
    *Description = &g_WaveFilterDescriptor;
    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS)
CMiniportWaveRT::DataRangeIntersection(_In_ ULONG PinId, _In_ PKSDATARANGE DataRange,
    _In_ PKSDATARANGE MatchingDataRange, _In_ ULONG OutputBufferLength,
    _Out_writes_bytes_to_opt_(OutputBufferLength, *ResultantFormatLength) PVOID ResultantFormat,
    _Out_ PULONG ResultantFormatLength)
{
    UNREFERENCED_PARAMETER(PinId);
    UNREFERENCED_PARAMETER(DataRange);
    UNREFERENCED_PARAMETER(MatchingDataRange);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(ResultantFormat);
    // Phase 2 TODO: build a KSDATAFORMAT_WAVEFORMATEX intersection. The default
    // PortCls handling suffices to load; format negotiation comes with DMA.
    *ResultantFormatLength = 0;
    return STATUS_NOT_IMPLEMENTED;
}

STDMETHODIMP_(NTSTATUS)
CMiniportWaveRT::Init(_In_ PUNKNOWN UnknownAdapter, _In_ PRESOURCELIST ResourceList, _In_ PPORTWAVERT Port)
{
    UNREFERENCED_PARAMETER(UnknownAdapter);
    UNREFERENCED_PARAMETER(ResourceList);
    PAGED_CODE();
    m_Port = Port;
    m_Port->AddRef();
    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS)
CMiniportWaveRT::NewStream(_Out_ PMINIPORTWAVERTSTREAM * Stream, _In_ PPORTWAVERTSTREAM PortStream,
    _In_ ULONG Pin, _In_ BOOLEAN Capture, _In_ PKSDATAFORMAT DataFormat)
{
    PAGED_CODE();
    if (Capture) {
        return STATUS_NOT_SUPPORTED;   // render-only skeleton
    }

    CMiniportWaveRTStream * stream =
        new(NonPagedPoolNx, RP1AUD_POOLTAG) CMiniportWaveRTStream(NULL);
    if (stream == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    stream->AddRef();

    NTSTATUS status = stream->Init(PortStream, Pin, DataFormat);
    if (NT_SUCCESS(status)) {
        *Stream = (PMINIPORTWAVERTSTREAM)stream;   // hand ref to caller
    } else {
        stream->Release();
    }
    return status;
}

STDMETHODIMP_(NTSTATUS)
CMiniportWaveRT::GetDeviceDescription(_Out_ PDEVICE_DESCRIPTION DeviceDescription)
{
    PAGED_CODE();
    RtlZeroMemory(DeviceDescription, sizeof(DEVICE_DESCRIPTION));
    DeviceDescription->Version          = DEVICE_DESCRIPTION_VERSION;
    DeviceDescription->Master           = TRUE;
    DeviceDescription->ScatterGather    = TRUE;
    DeviceDescription->Dma32BitAddresses = TRUE;
    DeviceDescription->InterfaceType    = PNPBus;
    DeviceDescription->MaximumLength    = 0xFFFFFFFF;
    return STATUS_SUCCESS;
}

// ---- factory ----
NTSTATUS CreateRp1WaveRTMiniport(_Out_ PUNKNOWN * OutUnknown, _In_opt_ PUNKNOWN OuterUnknown)
{
    PAGED_CODE();
    CMiniportWaveRT * obj = new(NonPagedPoolNx, RP1AUD_POOLTAG) CMiniportWaveRT(OuterUnknown);
    if (obj == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    *OutUnknown = (PUNKNOWN)(PMINIPORTWAVERT)obj;
    (*OutUnknown)->AddRef();
    return STATUS_SUCCESS;
}
