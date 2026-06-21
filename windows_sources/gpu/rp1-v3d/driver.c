/*++
Module Name: driver.c
Abstract:    DriverEntry for the RP1/V3D WDDM render KMD scaffold. Registers via
             DxgkInitialize + DRIVER_INITIALIZATION_DATA. Callbacks are forward-
             declared via WDK function typedefs (exact signatures); their stub
             implementations come in ddi.c (next stage). This file compiles to
             prove the render registration path; a functional driver also needs
             a UMD and the physical V3D.
--*/
#include "common.h"

DRIVER_INITIALIZE DriverEntry;

// PnP / power (shared with DOD)
DXGKDDI_ADD_DEVICE                      V3dAddDevice;
DXGKDDI_START_DEVICE                    V3dStartDevice;
DXGKDDI_STOP_DEVICE                     V3dStopDevice;
DXGKDDI_REMOVE_DEVICE                   V3dRemoveDevice;
DXGKDDI_DISPATCH_IO_REQUEST            V3dDispatchIoRequest;
DXGKDDI_INTERRUPT_ROUTINE              V3dInterruptRoutine;
DXGKDDI_DPC_ROUTINE                    V3dDpcRoutine;
DXGKDDI_QUERY_CHILD_RELATIONS          V3dQueryChildRelations;
DXGKDDI_QUERY_CHILD_STATUS             V3dQueryChildStatus;
DXGKDDI_QUERY_DEVICE_DESCRIPTOR        V3dQueryDeviceDescriptor;
DXGKDDI_SET_POWER_STATE                V3dSetPowerState;
DXGKDDI_QUERYADAPTERINFO               V3dQueryAdapterInfo;
// render core
DXGKDDI_CREATEDEVICE                   V3dCreateDevice;
DXGKDDI_CREATEALLOCATION               V3dCreateAllocation;
DXGKDDI_DESTROYALLOCATION              V3dDestroyAllocation;
DXGKDDI_DESCRIBEALLOCATION             V3dDescribeAllocation;
DXGKDDI_GETSTANDARDALLOCATIONDRIVERDATA V3dGetStandardAllocationDriverData;
DXGKDDI_PATCH                          V3dPatch;
DXGKDDI_SUBMITCOMMAND                  V3dSubmitCommand;
DXGKDDI_BUILDPAGINGBUFFER              V3dBuildPagingBuffer;
DXGKDDI_QUERYCURRENTFENCE              V3dQueryCurrentFence;
DXGKDDI_DESTROYDEVICE                  V3dDestroyDevice;
DXGKDDI_OPENALLOCATIONINFO             V3dOpenAllocation;
DXGKDDI_CLOSEALLOCATION                V3dCloseAllocation;
DXGKDDI_RENDER                         V3dRender;
DXGKDDI_PRESENT                        V3dPresent;
DXGKDDI_CREATECONTEXT                  V3dCreateContext;
DXGKDDI_DESTROYCONTEXT                 V3dDestroyContext;
// VidPn (display side of a full WDDM driver)
DXGKDDI_ISSUPPORTEDVIDPN               V3dIsSupportedVidPn;
DXGKDDI_RECOMMENDFUNCTIONALVIDPN       V3dRecommendFunctionalVidPn;
DXGKDDI_ENUMVIDPNCOFUNCMODALITY        V3dEnumVidPnCofuncModality;
DXGKDDI_SETVIDPNSOURCEADDRESS          V3dSetVidPnSourceAddress;
DXGKDDI_SETVIDPNSOURCEVISIBILITY       V3dSetVidPnSourceVisibility;
DXGKDDI_COMMITVIDPN                    V3dCommitVidPn;
DXGKDDI_UPDATEACTIVEVIDPNPRESENTPATH   V3dUpdateActiveVidPnPresentPath;
DXGKDDI_RECOMMENDMONITORMODES          V3dRecommendMonitorModes;
DXGKDDI_QUERYVIDPNHWCAPABILITY         V3dQueryVidPnHWCapability;
DXGKDDI_STOP_DEVICE_AND_RELEASE_POST_DISPLAY_OWNERSHIP V3dStopDeviceAndReleasePostDisplayOwnership;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#endif

NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    DRIVER_INITIALIZATION_DATA init;

    RtlZeroMemory(&init, sizeof(init));
    init.Version = DXGKDDI_INTERFACE_VERSION;

    init.DxgkDdiAddDevice                 = V3dAddDevice;
    init.DxgkDdiStartDevice               = V3dStartDevice;
    init.DxgkDdiStopDevice                = V3dStopDevice;
    init.DxgkDdiRemoveDevice              = V3dRemoveDevice;
    init.DxgkDdiDispatchIoRequest         = V3dDispatchIoRequest;
    init.DxgkDdiInterruptRoutine          = V3dInterruptRoutine;
    init.DxgkDdiDpcRoutine                = V3dDpcRoutine;
    init.DxgkDdiQueryChildRelations       = V3dQueryChildRelations;
    init.DxgkDdiQueryChildStatus          = V3dQueryChildStatus;
    init.DxgkDdiQueryDeviceDescriptor     = V3dQueryDeviceDescriptor;
    init.DxgkDdiSetPowerState             = V3dSetPowerState;
    init.DxgkDdiQueryAdapterInfo          = V3dQueryAdapterInfo;
    init.DxgkDdiCreateDevice              = V3dCreateDevice;
    init.DxgkDdiCreateAllocation          = V3dCreateAllocation;
    init.DxgkDdiDestroyAllocation         = V3dDestroyAllocation;
    init.DxgkDdiDescribeAllocation        = V3dDescribeAllocation;
    init.DxgkDdiGetStandardAllocationDriverData = V3dGetStandardAllocationDriverData;
    init.DxgkDdiPatch                     = V3dPatch;
    init.DxgkDdiSubmitCommand             = V3dSubmitCommand;
    init.DxgkDdiBuildPagingBuffer         = V3dBuildPagingBuffer;
    init.DxgkDdiQueryCurrentFence         = V3dQueryCurrentFence;
    init.DxgkDdiDestroyDevice             = V3dDestroyDevice;
    init.DxgkDdiOpenAllocation            = V3dOpenAllocation;
    init.DxgkDdiCloseAllocation           = V3dCloseAllocation;
    init.DxgkDdiRender                    = V3dRender;
    init.DxgkDdiPresent                   = V3dPresent;
    init.DxgkDdiCreateContext             = V3dCreateContext;
    init.DxgkDdiDestroyContext            = V3dDestroyContext;
    init.DxgkDdiIsSupportedVidPn          = V3dIsSupportedVidPn;
    init.DxgkDdiRecommendFunctionalVidPn  = V3dRecommendFunctionalVidPn;
    init.DxgkDdiEnumVidPnCofuncModality   = V3dEnumVidPnCofuncModality;
    init.DxgkDdiSetVidPnSourceAddress     = V3dSetVidPnSourceAddress;
    init.DxgkDdiSetVidPnSourceVisibility  = V3dSetVidPnSourceVisibility;
    init.DxgkDdiCommitVidPn               = V3dCommitVidPn;
    init.DxgkDdiUpdateActiveVidPnPresentPath = V3dUpdateActiveVidPnPresentPath;
    init.DxgkDdiRecommendMonitorModes     = V3dRecommendMonitorModes;
    init.DxgkDdiQueryVidPnHWCapability    = V3dQueryVidPnHWCapability;
    init.DxgkDdiStopDeviceAndReleasePostDisplayOwnership = V3dStopDeviceAndReleasePostDisplayOwnership;

    return DxgkInitialize(DriverObject, RegistryPath, &init);
}
