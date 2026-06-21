/*++

Module Name:
    driver.c

Abstract:
    DriverEntry / AddDevice / Unload for the RP1 I2S WDM skeleton.

--*/

#include "driver.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, Rp1I2sAddDevice)
#pragma alloc_text(PAGE, Rp1I2sUnload)
#endif

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    UNREFERENCED_PARAMETER(RegistryPath);

    DriverObject->DriverExtension->AddDevice        = Rp1I2sAddDevice;
    DriverObject->DriverUnload                       = Rp1I2sUnload;

    DriverObject->MajorFunction[IRP_MJ_PNP]             = Rp1I2sDispatchPnp;
    DriverObject->MajorFunction[IRP_MJ_POWER]           = Rp1I2sDispatchPower;
    DriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL]  = Rp1I2sDispatchSystemControl;
    DriverObject->MajorFunction[IRP_MJ_CREATE]          = Rp1I2sDispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]           = Rp1I2sDispatchCreateClose;

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
               "rp1i2s: DriverEntry (Phase 1 WDM skeleton)\n");
    return STATUS_SUCCESS;
}

NTSTATUS
Rp1I2sAddDevice(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PDEVICE_OBJECT PhysicalDeviceObject
    )
{
    NTSTATUS                  status;
    PDEVICE_OBJECT            fdo = NULL;
    PRP1I2S_DEVICE_EXTENSION  ext;

    PAGED_CODE();

    status = IoCreateDevice(DriverObject,
                            sizeof(RP1I2S_DEVICE_EXTENSION),
                            NULL,
                            FILE_DEVICE_UNKNOWN,
                            FILE_DEVICE_SECURE_OPEN,
                            FALSE,
                            &fdo);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    ext = (PRP1I2S_DEVICE_EXTENSION)fdo->DeviceExtension;
    RtlZeroMemory(ext, sizeof(*ext));
    ext->Self = fdo;
    ext->Pdo  = PhysicalDeviceObject;
    IoInitializeRemoveLock(&ext->RemoveLock, RP1_POOL_TAG, 0, 0);

    ext->LowerDevice = IoAttachDeviceToDeviceStack(fdo, PhysicalDeviceObject);
    if (ext->LowerDevice == NULL) {
        IoDeleteDevice(fdo);
        return STATUS_DEVICE_REMOVED;
    }

    fdo->Flags |= DO_BUFFERED_IO | DO_POWER_PAGABLE;
    fdo->Flags &= ~DO_DEVICE_INITIALIZING;

    return STATUS_SUCCESS;
}

VOID
Rp1I2sUnload(
    _In_ PDRIVER_OBJECT DriverObject
    )
{
    UNREFERENCED_PARAMETER(DriverObject);
    PAGED_CODE();
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "rp1i2s: Unload\n");
}

NTSTATUS
Rp1I2sDispatchCreateClose(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
    )
{
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}
