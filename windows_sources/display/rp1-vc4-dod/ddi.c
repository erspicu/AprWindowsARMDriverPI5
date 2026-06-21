/*++
Module Name: ddi.c
Abstract:    Minimal DXGK display-only DDI callback implementations (Stage A).
             Goal of this stage: compile + link + register a single HDMI child.
             Real modeset / framebuffer present is Stage B (needs vc4 HW + device).
             Signatures use the WDK IN_/OUT_ macros exactly as in dispmprt.h /
             d3dkmddi.h so they match the forward typedefs in driver.c.
--*/
#include "common.h"

NTSTATUS
DodAddDevice(IN_CONST_PDEVICE_OBJECT PhysicalDeviceObject, OUT_PPVOID MiniportDeviceContext)
{
    PRP1DOD_DEVICE dev;
    UNREFERENCED_PARAMETER(PhysicalDeviceObject);
    dev = (PRP1DOD_DEVICE)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(RP1DOD_DEVICE), RP1DOD_POOLTAG);
    if (dev == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    *MiniportDeviceContext = dev;
    return STATUS_SUCCESS;
}

NTSTATUS
DodStartDevice(IN_CONST_PVOID MiniportDeviceContext, IN_PDXGK_START_INFO DxgkStartInfo,
    IN_PDXGKRNL_INTERFACE DxgkInterface, OUT_PULONG NumberOfVideoPresentSources, OUT_PULONG NumberOfChildren)
{
    PRP1DOD_DEVICE dev = (PRP1DOD_DEVICE)MiniportDeviceContext;
    UNREFERENCED_PARAMETER(DxgkStartInfo);
    dev->DxgkInterface = *DxgkInterface;
    *NumberOfVideoPresentSources = 1;   // one HDMI source
    *NumberOfChildren = 1;              // one HDMI child/target
    return STATUS_SUCCESS;
}

NTSTATUS DodStopDevice(IN_CONST_PVOID MiniportDeviceContext)
{
    UNREFERENCED_PARAMETER(MiniportDeviceContext);
    return STATUS_SUCCESS;
}

NTSTATUS DodRemoveDevice(IN_CONST_PVOID MiniportDeviceContext)
{
    if (MiniportDeviceContext != NULL) {
        ExFreePoolWithTag(MiniportDeviceContext, RP1DOD_POOLTAG);
    }
    return STATUS_SUCCESS;
}

NTSTATUS
DodQueryChildRelations(IN_CONST_PVOID MiniportDeviceContext,
    PDXGK_CHILD_DESCRIPTOR ChildRelations, IN_ULONG ChildRelationsSize)
{
    UNREFERENCED_PARAMETER(MiniportDeviceContext);
    // Last entry is a terminator; we need room for 1 child + terminator.
    if (ChildRelationsSize >= 2 * sizeof(DXGK_CHILD_DESCRIPTOR)) {
        ChildRelations[0].ChildDeviceType = TypeVideoOutput;
        ChildRelations[0].ChildCapabilities.Type.VideoOutput.InterfaceTechnology = D3DKMDT_VOT_HDMI;
        ChildRelations[0].ChildCapabilities.Type.VideoOutput.MonitorOrientationAwareness = D3DKMDT_MOA_NONE;
        ChildRelations[0].ChildCapabilities.Type.VideoOutput.SupportsSdtvModes = FALSE;
        ChildRelations[0].ChildCapabilities.HpdAwareness = HpdAwarenessAlwaysConnected;
        ChildRelations[0].AcpiUid = 0;
        ChildRelations[0].ChildUid = 0;
    }
    return STATUS_SUCCESS;
}

NTSTATUS
DodQueryChildStatus(IN_CONST_PVOID MiniportDeviceContext, INOUT_PDXGK_CHILD_STATUS ChildStatus,
    IN_BOOLEAN NonDestructiveOnly)
{
    UNREFERENCED_PARAMETER(MiniportDeviceContext);
    UNREFERENCED_PARAMETER(NonDestructiveOnly);
    if (ChildStatus->Type == StatusConnection) {
        ChildStatus->HotPlug.Connected = TRUE;   // assume HDMI connected
    }
    return STATUS_SUCCESS;
}

NTSTATUS
DodQueryDeviceDescriptor(IN_CONST_PVOID MiniportDeviceContext, IN_ULONG ChildUid,
    INOUT_PDXGK_DEVICE_DESCRIPTOR DeviceDescriptor)
{
    UNREFERENCED_PARAMETER(MiniportDeviceContext);
    UNREFERENCED_PARAMETER(ChildUid);
    UNREFERENCED_PARAMETER(DeviceDescriptor);
    return STATUS_MONITOR_NO_DESCRIPTOR;   // let dxgk synthesize default modes
}

NTSTATUS
DodSetPowerState(IN_CONST_PVOID MiniportDeviceContext, IN_ULONG DeviceUid,
    IN_DEVICE_POWER_STATE DevicePowerState, IN_POWER_ACTION ActionType)
{
    UNREFERENCED_PARAMETER(MiniportDeviceContext);
    UNREFERENCED_PARAMETER(DeviceUid);
    UNREFERENCED_PARAMETER(DevicePowerState);
    UNREFERENCED_PARAMETER(ActionType);
    return STATUS_SUCCESS;
}

NTSTATUS APIENTRY
DodQueryAdapterInfo(IN_CONST_HANDLE hAdapter, IN_CONST_PDXGKARG_QUERYADAPTERINFO pQueryAdapterInfo)
{
    UNREFERENCED_PARAMETER(hAdapter);
    UNREFERENCED_PARAMETER(pQueryAdapterInfo);
    return STATUS_NOT_IMPLEMENTED;   // Stage B: report DriverCaps / etc.
}

NTSTATUS APIENTRY
DodIsSupportedVidPn(IN_CONST_HANDLE hAdapter, INOUT_PDXGKARG_ISSUPPORTEDVIDPN pIsSupportedVidPn)
{
    UNREFERENCED_PARAMETER(hAdapter);
    pIsSupportedVidPn->IsVidPnSupported = TRUE;   // Stage B: validate properly
    return STATUS_SUCCESS;
}

NTSTATUS APIENTRY
DodRecommendFunctionalVidPn(IN_CONST_HANDLE hAdapter, IN_CONST_PDXGKARG_RECOMMENDFUNCTIONALVIDPN_CONST pRecommendFunctionalVidPn)
{
    UNREFERENCED_PARAMETER(hAdapter);
    UNREFERENCED_PARAMETER(pRecommendFunctionalVidPn);
    return STATUS_GRAPHICS_NO_RECOMMENDED_FUNCTIONAL_VIDPN;
}

NTSTATUS APIENTRY
DodEnumVidPnCofuncModality(IN_CONST_HANDLE hAdapter, IN_CONST_PDXGKARG_ENUMVIDPNCOFUNCMODALITY_CONST pEnumCofuncModality)
{
    UNREFERENCED_PARAMETER(hAdapter);
    UNREFERENCED_PARAMETER(pEnumCofuncModality);
    return STATUS_SUCCESS;   // Stage B: pin source/target modes
}

NTSTATUS APIENTRY
DodSetVidPnSourceVisibility(IN_CONST_HANDLE hAdapter, IN_CONST_PDXGKARG_SETVIDPNSOURCEVISIBILITY pSetVidPnSourceVisibility)
{
    UNREFERENCED_PARAMETER(hAdapter);
    UNREFERENCED_PARAMETER(pSetVidPnSourceVisibility);
    return STATUS_SUCCESS;
}

NTSTATUS APIENTRY
DodCommitVidPn(IN_CONST_HANDLE hAdapter, IN_CONST_PDXGKARG_COMMITVIDPN_CONST pCommitVidPn)
{
    UNREFERENCED_PARAMETER(hAdapter);
    UNREFERENCED_PARAMETER(pCommitVidPn);
    return STATUS_SUCCESS;   // Stage B: program vc4 HVS/PixelValve/HDMI here
}

NTSTATUS APIENTRY
DodUpdateActiveVidPnPresentPath(IN_CONST_HANDLE hAdapter, IN_CONST_PDXGKARG_UPDATEACTIVEVIDPNPRESENTPATH_CONST pUpdateActiveVidPnPresentPath)
{
    UNREFERENCED_PARAMETER(hAdapter);
    UNREFERENCED_PARAMETER(pUpdateActiveVidPnPresentPath);
    return STATUS_SUCCESS;
}

NTSTATUS APIENTRY
DodRecommendMonitorModes(IN_CONST_HANDLE hAdapter, IN_CONST_PDXGKARG_RECOMMENDMONITORMODES_CONST pRecommendMonitorModes)
{
    UNREFERENCED_PARAMETER(hAdapter);
    UNREFERENCED_PARAMETER(pRecommendMonitorModes);
    return STATUS_SUCCESS;
}

NTSTATUS APIENTRY
DodQueryVidPnHWCapability(IN_CONST_HANDLE i_hAdapter, INOUT_PDXGKARG_QUERYVIDPNHWCAPABILITY io_pVidPnHWCaps)
{
    UNREFERENCED_PARAMETER(i_hAdapter);
    // All-zero HW caps => let the OS do scaling/rotation in software.
    io_pVidPnHWCaps->VidPnHWCaps.DriverRotation             = FALSE;
    io_pVidPnHWCaps->VidPnHWCaps.DriverScaling              = FALSE;
    io_pVidPnHWCaps->VidPnHWCaps.DriverCloning              = FALSE;
    io_pVidPnHWCaps->VidPnHWCaps.DriverColorConvert         = FALSE;
    io_pVidPnHWCaps->VidPnHWCaps.DriverLinkedAdapaterOutput = FALSE;
    io_pVidPnHWCaps->VidPnHWCaps.DriverRemoteDisplay        = FALSE;
    return STATUS_SUCCESS;
}

NTSTATUS APIENTRY
DodPresentDisplayOnly(IN_CONST_HANDLE hAdapter, IN_CONST_PDXGKARG_PRESENT_DISPLAYONLY pPresentDisplayOnly)
{
    UNREFERENCED_PARAMETER(hAdapter);
    UNREFERENCED_PARAMETER(pPresentDisplayOnly);
    return STATUS_SUCCESS;   // Stage B: blit source bitmap to the HDMI framebuffer
}

NTSTATUS
DodStopDeviceAndReleasePostDisplayOwnership(_In_ PVOID MiniportDeviceContext,
    _In_ D3DDDI_VIDEO_PRESENT_TARGET_ID TargetId, _Out_ PDXGK_DISPLAY_INFORMATION DisplayInfo)
{
    UNREFERENCED_PARAMETER(MiniportDeviceContext);
    UNREFERENCED_PARAMETER(TargetId);
    RtlZeroMemory(DisplayInfo, sizeof(DXGK_DISPLAY_INFORMATION));
    return STATUS_SUCCESS;
}
