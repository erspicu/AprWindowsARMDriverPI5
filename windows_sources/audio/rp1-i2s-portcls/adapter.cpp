/*++
Module Name: adapter.cpp
Abstract:    PortCls adapter: DriverEntry / AddDevice / StartDevice. Installs the
             Wave (PortWaveRT) and Topology subdevices and connects them.
--*/
#include "common.h"

#define MAX_MINIPORTS 2

DRIVER_ADD_DEVICE Rp1AudAddDevice;
extern "C" DRIVER_INITIALIZE DriverEntry;

static NTSTATUS
Rp1AudStartDevice(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp, _In_ PRESOURCELIST ResourceList)
{
    PAGED_CODE();

    NTSTATUS  status;
    PPORT     portWave = NULL;
    PPORT     portTopo = NULL;
    PUNKNOWN  miniWave = NULL;
    PUNKNOWN  miniTopo = NULL;

    // ---- Wave (WaveRT) subdevice ----
    status = PcNewPort(&portWave, CLSID_PortWaveRT);
    if (NT_SUCCESS(status)) {
        status = CreateRp1WaveRTMiniport(&miniWave, NULL);
    }
    if (NT_SUCCESS(status)) {
        status = portWave->Init(DeviceObject, Irp, miniWave, NULL, ResourceList);
    }
    if (NT_SUCCESS(status)) {
        status = PcRegisterSubdevice(DeviceObject, L"Wave", portWave);
    }

    // ---- Topology subdevice ----
    if (NT_SUCCESS(status)) {
        status = PcNewPort(&portTopo, CLSID_PortTopology);
    }
    if (NT_SUCCESS(status)) {
        status = CreateRp1TopoMiniport(&miniTopo, NULL);
    }
    if (NT_SUCCESS(status)) {
        status = portTopo->Init(DeviceObject, Irp, miniTopo, NULL, ResourceList);
    }
    if (NT_SUCCESS(status)) {
        status = PcRegisterSubdevice(DeviceObject, L"Topology", portTopo);
    }

    // ---- Physical connection: Wave bridge pin (1) -> Topology bridge pin (0) ----
    if (NT_SUCCESS(status)) {
        status = PcRegisterPhysicalConnection(DeviceObject, miniWave, 1, miniTopo, 0);
    }

    if (miniWave) miniWave->Release();
    if (miniTopo) miniTopo->Release();
    if (portWave) portWave->Release();
    if (portTopo) portTopo->Release();

    return status;
}

NTSTATUS
Rp1AudAddDevice(_In_ PDRIVER_OBJECT DriverObject, _In_ PDEVICE_OBJECT PhysicalDeviceObject)
{
    PAGED_CODE();
    return PcAddAdapterDevice(DriverObject,
                              PhysicalDeviceObject,
                              Rp1AudStartDevice,
                              MAX_MINIPORTS,
                              0);
}

extern "C" NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    return PcInitializeAdapterDriver(DriverObject, RegistryPath, Rp1AudAddDevice);
}
