/*++
Module Name: minwavestream.cpp
Abstract:    CMiniportWaveRTStream implementation. SetState drives the ported
             I2S HAL (Rp1I2sHwStart/Stop). The cyclic audio buffer is allocated
             via the WaveRT port's AllocatePagesForMdl.
--*/
#include "minwavestream.h"

CMiniportWaveRTStream::~CMiniportWaveRTStream()
{
    if (m_BufferMdl != NULL && m_Port != NULL) {
        m_Port->FreePagesFromMdl(m_BufferMdl);
        m_BufferMdl = NULL;
    }
    if (m_Port) {
        m_Port->Release();
        m_Port = NULL;
    }
}

STDMETHODIMP_(NTSTATUS)
CMiniportWaveRTStream::NonDelegatingQueryInterface(_In_ REFIID Interface, _COM_Outptr_ PVOID * Object)
{
    if (IsEqualGUIDAligned(Interface, IID_IUnknown)) {
        *Object = (PVOID)(PUNKNOWN)(PMINIPORTWAVERTSTREAM)this;
    } else if (IsEqualGUIDAligned(Interface, IID_IMiniportWaveRTStream)) {
        *Object = (PVOID)(PMINIPORTWAVERTSTREAM)this;
    } else {
        *Object = NULL;
    }
    if (*Object) {
        ((PUNKNOWN)*Object)->AddRef();
        return STATUS_SUCCESS;
    }
    return STATUS_INVALID_PARAMETER;
}

NTSTATUS
CMiniportWaveRTStream::Init(_In_ PPORTWAVERTSTREAM PortStream, _In_ ULONG Pin, _In_ PKSDATAFORMAT DataFormat)
{
    UNREFERENCED_PARAMETER(Pin);
    UNREFERENCED_PARAMETER(DataFormat);
    m_Port = PortStream;
    m_Port->AddRef();
    // No real RP1 MMIO yet (device sits under RP1->PCIe). Keep HAL inert.
    RtlZeroMemory(&m_Hw, sizeof(m_Hw));
    m_Position = 0;
    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS)
CMiniportWaveRTStream::SetFormat(_In_ PKSDATAFORMAT DataFormat)
{
    UNREFERENCED_PARAMETER(DataFormat);
    if (m_Hw.Base != NULL) {
        Rp1I2sHwSetResolution(&m_Hw, 16);
    }
    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS)
CMiniportWaveRTStream::SetState(_In_ KSSTATE State)
{
    if (m_Hw.Base != NULL) {
        if (State == KSSTATE_RUN) {
            Rp1I2sHwFlush(&m_Hw, TRUE);
            Rp1I2sHwConfig(&m_Hw, TRUE, 2);
            Rp1I2sHwStart(&m_Hw, TRUE);
        } else {
            Rp1I2sHwStop(&m_Hw, TRUE);
        }
    }
    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS)
CMiniportWaveRTStream::GetPosition(_Out_ PKSAUDIO_POSITION Position)
{
    // Skeleton: no DMA position counter yet.
    Position->PlayOffset  = m_Position;
    Position->WriteOffset = m_Position;
    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS)
CMiniportWaveRTStream::AllocateAudioBuffer(_In_ ULONG RequestedSize, _Out_ PMDL * AudioBufferMdl,
    _Out_ ULONG * ActualSize, _Out_ ULONG * OffsetFromFirstPage, _Out_ MEMORY_CACHING_TYPE * CacheType)
{
    PHYSICAL_ADDRESS high;
    high.QuadPart = -1;   // all-ones: no upper physical-address limit

    if (RequestedSize == 0) {
        RequestedSize = PAGE_SIZE;
    }
    RequestedSize = (RequestedSize + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    PMDL mdl = m_Port->AllocatePagesForMdl(high, RequestedSize);
    if (mdl == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    m_BufferMdl         = mdl;
    m_BufferSize        = RequestedSize;
    *AudioBufferMdl     = mdl;
    *ActualSize         = RequestedSize;
    *OffsetFromFirstPage = 0;
    *CacheType          = MmCached;
    return STATUS_SUCCESS;
}

STDMETHODIMP_(VOID)
CMiniportWaveRTStream::FreeAudioBuffer(_In_opt_ PMDL AudioBufferMdl, _In_ ULONG BufferSize)
{
    UNREFERENCED_PARAMETER(BufferSize);
    if (AudioBufferMdl != NULL) {
        m_Port->FreePagesFromMdl(AudioBufferMdl);
    }
    m_BufferMdl  = NULL;
    m_BufferSize = 0;
}

STDMETHODIMP_(VOID)
CMiniportWaveRTStream::GetHWLatency(_Out_ PKSRTAUDIO_HWLATENCY hwLatency)
{
    hwLatency->FifoSize     = 0;
    hwLatency->ChipsetDelay = 0;
    hwLatency->CodecDelay   = 0;
}

STDMETHODIMP_(NTSTATUS)
CMiniportWaveRTStream::GetPositionRegister(_Out_ PKSRTAUDIO_HWREGISTER Register)
{
    UNREFERENCED_PARAMETER(Register);
    return STATUS_NOT_IMPLEMENTED;   // fall back to GetPosition()
}

STDMETHODIMP_(NTSTATUS)
CMiniportWaveRTStream::GetClockRegister(_Out_ PKSRTAUDIO_HWREGISTER Register)
{
    UNREFERENCED_PARAMETER(Register);
    return STATUS_NOT_IMPLEMENTED;
}
