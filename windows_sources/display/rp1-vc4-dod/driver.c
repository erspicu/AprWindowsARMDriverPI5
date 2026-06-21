/*++
Module Name: driver.c
Abstract:    DriverEntry for the RP1/vc4 Display-Only Driver. Registers with
             dxgkrnl via DxgkInitializeDisplayOnlyDriver, wiring the mandatory
             DXGK display-only DDI callbacks.

    Stage A scaffold: callbacks are forward-declared here via their WDK function
    typedefs (which pin the exact signatures); their implementations live in
    ddi.c (added incrementally). Optional callbacks (interrupt/DPC/pointer/
    system-display) are left NULL for a minimal bring-up.
--*/
#include "common.h"

DRIVER_INITIALIZE DriverEntry;

// Mandatory DOD callbacks (signatures pinned by the WDK function typedefs).
DXGKDDI_ADD_DEVICE                                      DodAddDevice;
DXGKDDI_START_DEVICE                                    DodStartDevice;
DXGKDDI_STOP_DEVICE                                     DodStopDevice;
DXGKDDI_REMOVE_DEVICE                                   DodRemoveDevice;
DXGKDDI_QUERY_CHILD_RELATIONS                           DodQueryChildRelations;
DXGKDDI_QUERY_CHILD_STATUS                              DodQueryChildStatus;
DXGKDDI_QUERY_DEVICE_DESCRIPTOR                         DodQueryDeviceDescriptor;
DXGKDDI_SET_POWER_STATE                                 DodSetPowerState;
DXGKDDI_QUERYADAPTERINFO                                DodQueryAdapterInfo;
DXGKDDI_ISSUPPORTEDVIDPN                                DodIsSupportedVidPn;
DXGKDDI_RECOMMENDFUNCTIONALVIDPN                        DodRecommendFunctionalVidPn;
DXGKDDI_ENUMVIDPNCOFUNCMODALITY                         DodEnumVidPnCofuncModality;
DXGKDDI_SETVIDPNSOURCEVISIBILITY                        DodSetVidPnSourceVisibility;
DXGKDDI_COMMITVIDPN                                     DodCommitVidPn;
DXGKDDI_UPDATEACTIVEVIDPNPRESENTPATH                    DodUpdateActiveVidPnPresentPath;
DXGKDDI_RECOMMENDMONITORMODES                           DodRecommendMonitorModes;
DXGKDDI_QUERYVIDPNHWCAPABILITY                          DodQueryVidPnHWCapability;
DXGKDDI_PRESENTDISPLAYONLY                              DodPresentDisplayOnly;
DXGKDDI_STOP_DEVICE_AND_RELEASE_POST_DISPLAY_OWNERSHIP  DodStopDeviceAndReleasePostDisplayOwnership;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#endif

NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    KMDDOD_INITIALIZATION_DATA init;

    RtlZeroMemory(&init, sizeof(init));
    init.Version = DXGKDDI_INTERFACE_VERSION;   // WDDM 3.2 on WDK 26100

    init.DxgkDdiAddDevice                 = DodAddDevice;
    init.DxgkDdiStartDevice               = DodStartDevice;
    init.DxgkDdiStopDevice                = DodStopDevice;
    init.DxgkDdiRemoveDevice              = DodRemoveDevice;
    init.DxgkDdiQueryChildRelations       = DodQueryChildRelations;
    init.DxgkDdiQueryChildStatus          = DodQueryChildStatus;
    init.DxgkDdiQueryDeviceDescriptor     = DodQueryDeviceDescriptor;
    init.DxgkDdiSetPowerState             = DodSetPowerState;
    init.DxgkDdiQueryAdapterInfo          = DodQueryAdapterInfo;
    init.DxgkDdiIsSupportedVidPn          = DodIsSupportedVidPn;
    init.DxgkDdiRecommendFunctionalVidPn  = DodRecommendFunctionalVidPn;
    init.DxgkDdiEnumVidPnCofuncModality   = DodEnumVidPnCofuncModality;
    init.DxgkDdiSetVidPnSourceVisibility  = DodSetVidPnSourceVisibility;
    init.DxgkDdiCommitVidPn               = DodCommitVidPn;
    init.DxgkDdiUpdateActiveVidPnPresentPath = DodUpdateActiveVidPnPresentPath;
    init.DxgkDdiRecommendMonitorModes     = DodRecommendMonitorModes;
    init.DxgkDdiQueryVidPnHWCapability    = DodQueryVidPnHWCapability;
    init.DxgkDdiPresentDisplayOnly        = DodPresentDisplayOnly;
    init.DxgkDdiStopDeviceAndReleasePostDisplayOwnership = DodStopDeviceAndReleasePostDisplayOwnership;

    return DxgkInitializeDisplayOnlyDriver(DriverObject, RegistryPath, &init);
}
