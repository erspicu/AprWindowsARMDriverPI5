/*++
Module Name: ddi.c
Abstract:    Stub implementations of the 38 WDDM render/VidPn/PnP DDIs for the
             RP1/V3D KMD. These let the driver LINK (rp1v3d.sys). A functional
             render driver fills these in (V3D MMU/CL submission/fences) and ships
             a user-mode D3D driver (UMD); both need the physical V3D.
--*/
#include "common.h"

/* ---------------- PnP / power (dispmprt.h) ---------------- */

NTSTATUS V3dAddDevice(IN_CONST_PDEVICE_OBJECT PhysicalDeviceObject, OUT_PPVOID MiniportDeviceContext)
{
    PRP1V3D_ADAPTER adapter;
    UNREFERENCED_PARAMETER(PhysicalDeviceObject);
    adapter = (PRP1V3D_ADAPTER)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(RP1V3D_ADAPTER), RP1V3D_POOLTAG);
    if (adapter == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    *MiniportDeviceContext = adapter;
    return STATUS_SUCCESS;
}

NTSTATUS V3dStartDevice(IN_CONST_PVOID MiniportDeviceContext, IN_PDXGK_START_INFO DxgkStartInfo,
    IN_PDXGKRNL_INTERFACE DxgkInterface, OUT_PULONG NumberOfVideoPresentSources, OUT_PULONG NumberOfChildren)
{
    PRP1V3D_ADAPTER adapter = (PRP1V3D_ADAPTER)MiniportDeviceContext;
    DXGK_DEVICE_INFO devInfo;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR best = NULL;
    NTSTATUS status;

    UNREFERENCED_PARAMETER(DxgkStartInfo);
    if (adapter == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    adapter->DxgkInterface = *DxgkInterface;

    /* Ask dxgkrnl for our hardware resources (the V3D MMIO block). */
    RtlZeroMemory(&devInfo, sizeof(devInfo));
    status = adapter->DxgkInterface.DxgkCbGetDeviceInformation(
                 adapter->DxgkInterface.DeviceHandle, &devInfo);
    if (NT_SUCCESS(status) && devInfo.TranslatedResourceList != NULL) {
        ULONG i;
        PCM_PARTIAL_RESOURCE_LIST prl =
            &devInfo.TranslatedResourceList->List[0].PartialResourceList;
        for (i = 0; i < prl->Count; i++) {
            PCM_PARTIAL_RESOURCE_DESCRIPTOR d = &prl->PartialDescriptors[i];
            if (d->Type == CmResourceTypeMemory &&
                (best == NULL || d->u.Memory.Length > best->u.Memory.Length)) {
                best = d;   /* V3D reg block = the (largest) MMIO window */
            }
        }
    }
    if (best != NULL) {
        adapter->RegsPhys = best->u.Memory.Start;
        adapter->RegsLen  = best->u.Memory.Length;
        adapter->Regs     = (PUCHAR)MmMapIoSpaceEx(best->u.Memory.Start,
                                best->u.Memory.Length, PAGE_READWRITE | PAGE_NOCACHE);
        if (adapter->Regs != NULL) {
            /* Read V3D IDENT to prove we are talking to real hardware. */
            adapter->CoreIdent0 = V3dRd(adapter, V3D_CTL_IDENT0);
            adapter->HubIdent0  = V3dRd(adapter, V3D_HUB_IDENT0);
            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
                "rp1v3d: V3D regs @ %p (len 0x%Ix) CTL_IDENT0=0x%08x HUB_IDENT0=0x%08x VER=%u\n",
                adapter->Regs, adapter->RegsLen, adapter->CoreIdent0, adapter->HubIdent0,
                (adapter->CoreIdent0 >> 24) & 0xff);
        }
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_WARNING_LEVEL,
            "rp1v3d: no MMIO resource from dxgkrnl - V3D regs not mapped\n");
    }

    *NumberOfVideoPresentSources = 0;   /* render-only path; display via DOD */
    *NumberOfChildren            = 0;
    return STATUS_SUCCESS;
}

NTSTATUS V3dStopDevice(IN_CONST_PVOID MiniportDeviceContext)
{
    PRP1V3D_ADAPTER adapter = (PRP1V3D_ADAPTER)MiniportDeviceContext;
    if (adapter != NULL && adapter->Regs != NULL) {
        MmUnmapIoSpace(adapter->Regs, adapter->RegsLen);
        adapter->Regs = NULL;
    }
    return STATUS_SUCCESS;
}

NTSTATUS V3dRemoveDevice(IN_CONST_PVOID MiniportDeviceContext)
{
    if (MiniportDeviceContext != NULL) {
        ExFreePoolWithTag(MiniportDeviceContext, RP1V3D_POOLTAG);
    }
    return STATUS_SUCCESS;
}

NTSTATUS V3dDispatchIoRequest(IN_CONST_PVOID MiniportDeviceContext, IN_ULONG VidPnSourceId,
    IN_PVIDEO_REQUEST_PACKET VideoRequestPacket)
{
    UNREFERENCED_PARAMETER(MiniportDeviceContext);
    UNREFERENCED_PARAMETER(VidPnSourceId);
    UNREFERENCED_PARAMETER(VideoRequestPacket);
    return STATUS_NOT_IMPLEMENTED;
}

BOOLEAN V3dInterruptRoutine(IN_CONST_PVOID MiniportDeviceContext, IN_ULONG MessageNumber)
{
    UNREFERENCED_PARAMETER(MiniportDeviceContext);
    UNREFERENCED_PARAMETER(MessageNumber);
    return FALSE;
}

VOID V3dDpcRoutine(IN_CONST_PVOID MiniportDeviceContext)
{
    UNREFERENCED_PARAMETER(MiniportDeviceContext);
}

NTSTATUS V3dQueryChildRelations(IN_CONST_PVOID MiniportDeviceContext,
    PDXGK_CHILD_DESCRIPTOR ChildRelations, ULONG ChildRelationsSize)
{
    UNREFERENCED_PARAMETER(MiniportDeviceContext);
    UNREFERENCED_PARAMETER(ChildRelations);
    UNREFERENCED_PARAMETER(ChildRelationsSize);
    return STATUS_SUCCESS;
}

NTSTATUS V3dQueryChildStatus(IN_CONST_PVOID MiniportDeviceContext, INOUT_PDXGK_CHILD_STATUS ChildStatus,
    IN_BOOLEAN NonDestructiveOnly)
{
    UNREFERENCED_PARAMETER(MiniportDeviceContext);
    UNREFERENCED_PARAMETER(ChildStatus);
    UNREFERENCED_PARAMETER(NonDestructiveOnly);
    return STATUS_SUCCESS;
}

NTSTATUS V3dQueryDeviceDescriptor(IN_CONST_PVOID MiniportDeviceContext, IN_ULONG ChildUid,
    INOUT_PDXGK_DEVICE_DESCRIPTOR DeviceDescriptor)
{
    UNREFERENCED_PARAMETER(MiniportDeviceContext);
    UNREFERENCED_PARAMETER(ChildUid);
    UNREFERENCED_PARAMETER(DeviceDescriptor);
    return STATUS_MONITOR_NO_DESCRIPTOR;
}

NTSTATUS V3dSetPowerState(IN_CONST_PVOID MiniportDeviceContext, IN_ULONG DeviceUid,
    IN_DEVICE_POWER_STATE DevicePowerState, IN_POWER_ACTION ActionType)
{
    UNREFERENCED_PARAMETER(MiniportDeviceContext);
    UNREFERENCED_PARAMETER(DeviceUid);
    UNREFERENCED_PARAMETER(DevicePowerState);
    UNREFERENCED_PARAMETER(ActionType);
    return STATUS_SUCCESS;
}

NTSTATUS V3dQueryAdapterInfo(IN_CONST_HANDLE hAdapter, IN_CONST_PDXGKARG_QUERYADAPTERINFO pQueryAdapterInfo)
{
    UNREFERENCED_PARAMETER(hAdapter);

    switch (pQueryAdapterInfo->Type) {
    case DXGKQAITYPE_DRIVERCAPS:
        /* Minimal caps so dxgkrnl accepts a render-only adapter. Detailed caps
           (scheduling/paging/preemption) are filled when render lands. */
        if (pQueryAdapterInfo->OutputDataSize >= sizeof(DXGK_DRIVERCAPS)) {
            DXGK_DRIVERCAPS *caps = (DXGK_DRIVERCAPS *)pQueryAdapterInfo->pOutputData;
            RtlZeroMemory(caps, sizeof(DXGK_DRIVERCAPS));
            caps->HighestAcceptableAddress.QuadPart = (ULONGLONG)-1;  /* 64-bit DMA */
            caps->MaxAllocationListSlotId           = 1;
            return STATUS_SUCCESS;
        }
        return STATUS_BUFFER_TOO_SMALL;

    default:
        /* Render/scheduling/segment caps not yet provided (shell). */
        return STATUS_NOT_IMPLEMENTED;
    }
}

/* ---------------- render core (d3dkmddi.h) ---------------- */

NTSTATUS V3dCreateDevice(IN_CONST_HANDLE hAdapter, INOUT_PDXGKARG_CREATEDEVICE pCreateDevice)
{
    UNREFERENCED_PARAMETER(hAdapter);
    UNREFERENCED_PARAMETER(pCreateDevice);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS V3dCreateAllocation(IN_CONST_HANDLE hAdapter, INOUT_PDXGKARG_CREATEALLOCATION pCreateAllocation)
{
    UNREFERENCED_PARAMETER(hAdapter);
    UNREFERENCED_PARAMETER(pCreateAllocation);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS V3dDestroyAllocation(IN_CONST_HANDLE hAdapter, IN_CONST_PDXGKARG_DESTROYALLOCATION pDestroyAllocation)
{
    UNREFERENCED_PARAMETER(hAdapter);
    UNREFERENCED_PARAMETER(pDestroyAllocation);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS V3dDescribeAllocation(IN_CONST_HANDLE hAdapter, INOUT_PDXGKARG_DESCRIBEALLOCATION pDescribeAllocation)
{
    UNREFERENCED_PARAMETER(hAdapter);
    UNREFERENCED_PARAMETER(pDescribeAllocation);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS V3dGetStandardAllocationDriverData(IN_CONST_HANDLE hAdapter,
    INOUT_PDXGKARG_GETSTANDARDALLOCATIONDRIVERDATA pGetStandardAllocationDriverData)
{
    UNREFERENCED_PARAMETER(hAdapter);
    UNREFERENCED_PARAMETER(pGetStandardAllocationDriverData);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS V3dPatch(IN_CONST_HANDLE hAdapter, IN_CONST_PDXGKARG_PATCH pPatch)
{
    UNREFERENCED_PARAMETER(hAdapter);
    UNREFERENCED_PARAMETER(pPatch);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS V3dSubmitCommand(IN_CONST_HANDLE hAdapter, IN_CONST_PDXGKARG_SUBMITCOMMAND pSubmitCommand)
{
    UNREFERENCED_PARAMETER(hAdapter);
    UNREFERENCED_PARAMETER(pSubmitCommand);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS V3dBuildPagingBuffer(IN_CONST_HANDLE hAdapter, IN_PDXGKARG_BUILDPAGINGBUFFER pBuildPagingBuffer)
{
    UNREFERENCED_PARAMETER(hAdapter);
    UNREFERENCED_PARAMETER(pBuildPagingBuffer);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS V3dQueryCurrentFence(IN_CONST_HANDLE hAdapter, INOUT_PDXGKARG_QUERYCURRENTFENCE pCurrentFence)
{
    UNREFERENCED_PARAMETER(hAdapter);
    UNREFERENCED_PARAMETER(pCurrentFence);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS V3dDestroyDevice(IN_CONST_HANDLE hDevice)
{
    UNREFERENCED_PARAMETER(hDevice);
    return STATUS_SUCCESS;
}

NTSTATUS V3dOpenAllocation(IN_CONST_HANDLE hDevice, IN_CONST_PDXGKARG_OPENALLOCATION pOpenAllocation)
{
    UNREFERENCED_PARAMETER(hDevice);
    UNREFERENCED_PARAMETER(pOpenAllocation);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS V3dCloseAllocation(IN_CONST_HANDLE hDevice, IN_CONST_PDXGKARG_CLOSEALLOCATION pCloseAllocation)
{
    UNREFERENCED_PARAMETER(hDevice);
    UNREFERENCED_PARAMETER(pCloseAllocation);
    return STATUS_SUCCESS;
}

NTSTATUS V3dRender(IN_CONST_HANDLE hContext, INOUT_PDXGKARG_RENDER pRender)
{
    UNREFERENCED_PARAMETER(hContext);
    UNREFERENCED_PARAMETER(pRender);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS V3dPresent(IN_CONST_HANDLE hContext, INOUT_PDXGKARG_PRESENT pPresent)
{
    UNREFERENCED_PARAMETER(hContext);
    UNREFERENCED_PARAMETER(pPresent);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS V3dCreateContext(IN_CONST_HANDLE hDevice, INOUT_PDXGKARG_CREATECONTEXT pCreateContext)
{
    UNREFERENCED_PARAMETER(hDevice);
    UNREFERENCED_PARAMETER(pCreateContext);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS V3dDestroyContext(IN_CONST_HANDLE hContext)
{
    UNREFERENCED_PARAMETER(hContext);
    return STATUS_SUCCESS;
}

/* ---------------- VidPn (display side) (d3dkmddi.h) ---------------- */

NTSTATUS V3dIsSupportedVidPn(IN_CONST_HANDLE hAdapter, INOUT_PDXGKARG_ISSUPPORTEDVIDPN pIsSupportedVidPn)
{
    UNREFERENCED_PARAMETER(hAdapter);
    UNREFERENCED_PARAMETER(pIsSupportedVidPn);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS V3dRecommendFunctionalVidPn(IN_CONST_HANDLE hAdapter,
    IN_CONST_PDXGKARG_RECOMMENDFUNCTIONALVIDPN_CONST pRecommendFunctionalVidPn)
{
    UNREFERENCED_PARAMETER(hAdapter);
    UNREFERENCED_PARAMETER(pRecommendFunctionalVidPn);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS V3dEnumVidPnCofuncModality(IN_CONST_HANDLE hAdapter,
    IN_CONST_PDXGKARG_ENUMVIDPNCOFUNCMODALITY_CONST pEnumCofuncModality)
{
    UNREFERENCED_PARAMETER(hAdapter);
    UNREFERENCED_PARAMETER(pEnumCofuncModality);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS V3dSetVidPnSourceAddress(IN_CONST_HANDLE hAdapter,
    IN_CONST_PDXGKARG_SETVIDPNSOURCEADDRESS pSetVidPnSourceAddress)
{
    UNREFERENCED_PARAMETER(hAdapter);
    UNREFERENCED_PARAMETER(pSetVidPnSourceAddress);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS V3dSetVidPnSourceVisibility(IN_CONST_HANDLE hAdapter,
    IN_CONST_PDXGKARG_SETVIDPNSOURCEVISIBILITY pSetVidPnSourceVisibility)
{
    UNREFERENCED_PARAMETER(hAdapter);
    UNREFERENCED_PARAMETER(pSetVidPnSourceVisibility);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS V3dCommitVidPn(IN_CONST_HANDLE hAdapter, IN_CONST_PDXGKARG_COMMITVIDPN_CONST pCommitVidPn)
{
    UNREFERENCED_PARAMETER(hAdapter);
    UNREFERENCED_PARAMETER(pCommitVidPn);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS V3dUpdateActiveVidPnPresentPath(IN_CONST_HANDLE hAdapter,
    IN_CONST_PDXGKARG_UPDATEACTIVEVIDPNPRESENTPATH_CONST pUpdateActiveVidPnPresentPath)
{
    UNREFERENCED_PARAMETER(hAdapter);
    UNREFERENCED_PARAMETER(pUpdateActiveVidPnPresentPath);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS V3dRecommendMonitorModes(IN_CONST_HANDLE hAdapter,
    IN_CONST_PDXGKARG_RECOMMENDMONITORMODES_CONST pRecommendMonitorModes)
{
    UNREFERENCED_PARAMETER(hAdapter);
    UNREFERENCED_PARAMETER(pRecommendMonitorModes);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS V3dQueryVidPnHWCapability(IN_CONST_HANDLE i_hAdapter, INOUT_PDXGKARG_QUERYVIDPNHWCAPABILITY io_pVidPnHWCaps)
{
    UNREFERENCED_PARAMETER(i_hAdapter);
    UNREFERENCED_PARAMETER(io_pVidPnHWCaps);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS V3dStopDeviceAndReleasePostDisplayOwnership(_In_ PVOID MiniportDeviceContext,
    _In_ D3DDDI_VIDEO_PRESENT_TARGET_ID TargetId, _Out_ PDXGK_DISPLAY_INFORMATION DisplayInfo)
{
    UNREFERENCED_PARAMETER(MiniportDeviceContext);
    UNREFERENCED_PARAMETER(TargetId);
    UNREFERENCED_PARAMETER(DisplayInfo);
    return STATUS_NOT_IMPLEMENTED;
}
