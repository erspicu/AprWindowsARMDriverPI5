/*++
Module Name: miniport.c
Abstract:    NDIS 6.30 miniport for the RP1 Gigabit Ethernet (Cadence GEM/macb).
             DriverEntry registers the miniport; Initialize registers the adapter
             with NDIS. TX/RX DMA rings, PHY (MDIO) and general attributes
             (MAC/link) are the next stage and need real hardware.
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

    mp.InitializeHandlerEx          = GemInitialize;
    mp.HaltHandlerEx                = GemHalt;
    mp.PauseHandler                 = GemPause;
    mp.RestartHandler               = GemRestart;
    mp.OidRequestHandler            = GemOidRequest;
    mp.SendNetBufferListsHandler    = GemSendNetBufferLists;
    mp.ReturnNetBufferListsHandler  = GemReturnNetBufferLists;
    mp.CancelSendHandler            = GemCancelSend;
    mp.CheckForHangHandlerEx        = GemCheckForHang;
    mp.ResetHandlerEx               = GemReset;
    mp.DevicePnPEventNotifyHandler  = GemDevicePnpEventNotify;
    mp.ShutdownHandlerEx            = GemShutdown;
    mp.CancelOidRequestHandler      = GemCancelOidRequest;

    status = NdisMRegisterMiniportDriver(DriverObject, RegistryPath, NULL, &mp, &driverHandle);
    return (NTSTATUS)status;
}

NDIS_STATUS
GemInitialize(_In_ NDIS_HANDLE NdisMiniportHandle, _In_ NDIS_HANDLE MiniportDriverContext,
    _In_ PNDIS_MINIPORT_INIT_PARAMETERS MiniportInitParameters)
{
    PGEM_ADAPTER adapter;
    NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES regAttr;
    NDIS_STATUS  status;

    UNREFERENCED_PARAMETER(MiniportDriverContext);
    UNREFERENCED_PARAMETER(MiniportInitParameters);

    adapter = (PGEM_ADAPTER)NdisAllocateMemoryWithTagPriority(
                  NdisMiniportHandle, sizeof(GEM_ADAPTER), GEM_TAG, NormalPoolPriority);
    if (adapter == NULL) {
        return NDIS_STATUS_RESOURCES;
    }
    NdisZeroMemory(adapter, sizeof(GEM_ADAPTER));
    adapter->MiniportHandle = NdisMiniportHandle;

    NdisZeroMemory(&regAttr, sizeof(regAttr));
    regAttr.Header.Type       = NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES;
    regAttr.Header.Revision   = NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_1;
    regAttr.Header.Size       = NDIS_SIZEOF_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_1;
    regAttr.MiniportAdapterContext = (NDIS_HANDLE)adapter;
    regAttr.AttributeFlags    = NDIS_MINIPORT_ATTRIBUTES_HARDWARE_DEVICE | NDIS_MINIPORT_ATTRIBUTES_BUS_MASTER;
    regAttr.InterfaceType     = NdisInterfacePci;

    status = NdisMSetMiniportAttributes(NdisMiniportHandle,
                 (PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&regAttr);
    if (status != NDIS_STATUS_SUCCESS) {
        NdisFreeMemory(adapter, 0, 0);
        return status;
    }
    /* TODO: set general attributes (MediaType/link speed/MAC/OID list),
       map GEM MMIO from MiniportInitParameters->AllocatedResources, init TX/RX
       DMA rings + MDIO PHY. */
    return NDIS_STATUS_SUCCESS;
}

VOID
GemHalt(_In_ NDIS_HANDLE MiniportAdapterContext, _In_ NDIS_HALT_ACTION HaltAction)
{
    PGEM_ADAPTER adapter = (PGEM_ADAPTER)MiniportAdapterContext;
    UNREFERENCED_PARAMETER(HaltAction);
    if (adapter != NULL) {
        NdisFreeMemory(adapter, 0, 0);
    }
}

NDIS_STATUS
GemPause(_In_ NDIS_HANDLE MiniportAdapterContext, _In_ PNDIS_MINIPORT_PAUSE_PARAMETERS PauseParameters)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(PauseParameters);
    return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS
GemRestart(_In_ NDIS_HANDLE MiniportAdapterContext, _In_ PNDIS_MINIPORT_RESTART_PARAMETERS RestartParameters)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(RestartParameters);
    return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS
GemOidRequest(_In_ NDIS_HANDLE MiniportAdapterContext, _In_ PNDIS_OID_REQUEST OidRequest)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(OidRequest);
    /* TODO: handle mandatory query/set OIDs (stats, MAC, link state). */
    return NDIS_STATUS_NOT_SUPPORTED;
}

VOID
GemSendNetBufferLists(_In_ NDIS_HANDLE MiniportAdapterContext, _In_ PNET_BUFFER_LIST NetBufferList,
    _In_ NDIS_PORT_NUMBER PortNumber, _In_ ULONG SendFlags)
{
    PGEM_ADAPTER     adapter = (PGEM_ADAPTER)MiniportAdapterContext;
    PNET_BUFFER_LIST nbl = NetBufferList;
    UNREFERENCED_PARAMETER(PortNumber);
    UNREFERENCED_PARAMETER(SendFlags);
    /* No TX engine yet: fail the sends back so the stack does not stall. */
    while (nbl != NULL) {
        NET_BUFFER_LIST_STATUS(nbl) = NDIS_STATUS_FAILURE;
        nbl = NET_BUFFER_LIST_NEXT_NBL(nbl);
    }
    NdisMSendNetBufferListsComplete(adapter->MiniportHandle, NetBufferList, 0);
}

VOID
GemReturnNetBufferLists(_In_ NDIS_HANDLE MiniportAdapterContext, _In_ PNET_BUFFER_LIST NetBufferLists,
    _In_ ULONG ReturnFlags)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(NetBufferLists);
    UNREFERENCED_PARAMETER(ReturnFlags);
}

VOID
GemCancelSend(_In_ NDIS_HANDLE MiniportAdapterContext, _In_ PVOID CancelId)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(CancelId);
}

BOOLEAN
GemCheckForHang(_In_ NDIS_HANDLE MiniportAdapterContext)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    return FALSE;
}

NDIS_STATUS
GemReset(_In_ NDIS_HANDLE MiniportAdapterContext, _Out_ PBOOLEAN AddressingReset)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    *AddressingReset = FALSE;
    return NDIS_STATUS_SUCCESS;
}

VOID
GemDevicePnpEventNotify(_In_ NDIS_HANDLE MiniportAdapterContext, _In_ PNET_DEVICE_PNP_EVENT NetDevicePnPEvent)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(NetDevicePnPEvent);
}

VOID
GemShutdown(_In_ NDIS_HANDLE MiniportAdapterContext, _In_ NDIS_SHUTDOWN_ACTION ShutdownAction)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(ShutdownAction);
}

VOID
GemCancelOidRequest(_In_ NDIS_HANDLE MiniportAdapterContext, _In_ PVOID RequestId)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(RequestId);
}
