/*++
Module Name: miniport.c
Abstract:    NDIS miniport registration skeleton for the CYW43455 Wi-Fi adapter.
             See common.h for the honest scope note - the WDI/dot11 protocol
             layer, SDIO transport and firmware load are the (large) next stages.
--*/
#include "common.h"

NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    NDIS_MINIPORT_DRIVER_CHARACTERISTICS mp;
    NDIS_HANDLE  driverHandle = NULL;
    NDIS_STATUS  status;

    NdisZeroMemory(&mp, sizeof(mp));
    mp.Header.Type     = NDIS_OBJECT_TYPE_MINIPORT_DRIVER_CHARACTERISTICS;
    mp.Header.Revision = NDIS_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_2;
    mp.Header.Size     = NDIS_SIZEOF_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_2;
    mp.MajorNdisVersion   = 6;
    mp.MinorNdisVersion   = 30;
    mp.MajorDriverVersion = 1;
    mp.MinorDriverVersion = 0;

    mp.InitializeHandlerEx          = WlInitialize;
    mp.HaltHandlerEx                = WlHalt;
    mp.PauseHandler                 = WlPause;
    mp.RestartHandler               = WlRestart;
    mp.OidRequestHandler            = WlOidRequest;
    mp.SendNetBufferListsHandler    = WlSendNetBufferLists;
    mp.ReturnNetBufferListsHandler  = WlReturnNetBufferLists;
    mp.CancelSendHandler            = WlCancelSend;
    mp.CheckForHangHandlerEx        = WlCheckForHang;
    mp.ResetHandlerEx               = WlReset;
    mp.DevicePnPEventNotifyHandler  = WlDevicePnpEventNotify;
    mp.ShutdownHandlerEx            = WlShutdown;
    mp.CancelOidRequestHandler      = WlCancelOidRequest;

    status = NdisMRegisterMiniportDriver(DriverObject, RegistryPath, NULL, &mp, &driverHandle);
    return (NTSTATUS)status;
}

NDIS_STATUS
WlInitialize(_In_ NDIS_HANDLE NdisMiniportHandle, _In_ NDIS_HANDLE MiniportDriverContext,
    _In_ PNDIS_MINIPORT_INIT_PARAMETERS MiniportInitParameters)
{
    PWL_ADAPTER adapter;
    NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES regAttr;
    NDIS_STATUS status;

    UNREFERENCED_PARAMETER(MiniportDriverContext);
    UNREFERENCED_PARAMETER(MiniportInitParameters);

    adapter = (PWL_ADAPTER)NdisAllocateMemoryWithTagPriority(
                  NdisMiniportHandle, sizeof(WL_ADAPTER), WL_TAG, NormalPoolPriority);
    if (adapter == NULL) {
        return NDIS_STATUS_RESOURCES;
    }
    NdisZeroMemory(adapter, sizeof(WL_ADAPTER));
    adapter->MiniportHandle = NdisMiniportHandle;

    NdisZeroMemory(&regAttr, sizeof(regAttr));
    regAttr.Header.Type     = NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES;
    regAttr.Header.Revision = NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_1;
    regAttr.Header.Size     = NDIS_SIZEOF_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_1;
    regAttr.MiniportAdapterContext = (NDIS_HANDLE)adapter;
    regAttr.AttributeFlags  = NDIS_MINIPORT_ATTRIBUTES_HARDWARE_DEVICE;
    regAttr.InterfaceType   = NdisInterfaceInternal;   /* CYW43455 is SDIO-attached */

    status = NdisMSetMiniportAttributes(NdisMiniportHandle,
                 (PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&regAttr);
    if (status != NDIS_STATUS_SUCCESS) {
        NdisFreeMemory(adapter, 0, 0);
        return status;
    }
    /* TODO (large): set Native-802.11/WDI attributes, open the SDIO function,
       load CYW43455 firmware, then implement the dot11 OID/scan/connect surface. */
    return NDIS_STATUS_SUCCESS;
}

VOID
WlHalt(_In_ NDIS_HANDLE MiniportAdapterContext, _In_ NDIS_HALT_ACTION HaltAction)
{
    PWL_ADAPTER adapter = (PWL_ADAPTER)MiniportAdapterContext;
    UNREFERENCED_PARAMETER(HaltAction);
    if (adapter != NULL) {
        NdisFreeMemory(adapter, 0, 0);
    }
}

NDIS_STATUS
WlPause(_In_ NDIS_HANDLE MiniportAdapterContext, _In_ PNDIS_MINIPORT_PAUSE_PARAMETERS PauseParameters)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(PauseParameters);
    return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS
WlRestart(_In_ NDIS_HANDLE MiniportAdapterContext, _In_ PNDIS_MINIPORT_RESTART_PARAMETERS RestartParameters)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(RestartParameters);
    return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS
WlOidRequest(_In_ NDIS_HANDLE MiniportAdapterContext, _In_ PNDIS_OID_REQUEST OidRequest)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(OidRequest);
    return NDIS_STATUS_NOT_SUPPORTED;
}

VOID
WlSendNetBufferLists(_In_ NDIS_HANDLE MiniportAdapterContext, _In_ PNET_BUFFER_LIST NetBufferList,
    _In_ NDIS_PORT_NUMBER PortNumber, _In_ ULONG SendFlags)
{
    PWL_ADAPTER      adapter = (PWL_ADAPTER)MiniportAdapterContext;
    PNET_BUFFER_LIST nbl = NetBufferList;
    UNREFERENCED_PARAMETER(PortNumber);
    UNREFERENCED_PARAMETER(SendFlags);
    while (nbl != NULL) {
        NET_BUFFER_LIST_STATUS(nbl) = NDIS_STATUS_FAILURE;
        nbl = NET_BUFFER_LIST_NEXT_NBL(nbl);
    }
    NdisMSendNetBufferListsComplete(adapter->MiniportHandle, NetBufferList, 0);
}

VOID
WlReturnNetBufferLists(_In_ NDIS_HANDLE MiniportAdapterContext, _In_ PNET_BUFFER_LIST NetBufferLists,
    _In_ ULONG ReturnFlags)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(NetBufferLists);
    UNREFERENCED_PARAMETER(ReturnFlags);
}

VOID
WlCancelSend(_In_ NDIS_HANDLE MiniportAdapterContext, _In_ PVOID CancelId)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(CancelId);
}

BOOLEAN
WlCheckForHang(_In_ NDIS_HANDLE MiniportAdapterContext)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    return FALSE;
}

NDIS_STATUS
WlReset(_In_ NDIS_HANDLE MiniportAdapterContext, _Out_ PBOOLEAN AddressingReset)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    *AddressingReset = FALSE;
    return NDIS_STATUS_SUCCESS;
}

VOID
WlDevicePnpEventNotify(_In_ NDIS_HANDLE MiniportAdapterContext, _In_ PNET_DEVICE_PNP_EVENT NetDevicePnPEvent)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(NetDevicePnPEvent);
}

VOID
WlShutdown(_In_ NDIS_HANDLE MiniportAdapterContext, _In_ NDIS_SHUTDOWN_ACTION ShutdownAction)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(ShutdownAction);
}

VOID
WlCancelOidRequest(_In_ NDIS_HANDLE MiniportAdapterContext, _In_ PVOID RequestId)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(RequestId);
}
