/*++

Module Name:
    pnp.c

Abstract:
    PnP / Power dispatch for the RP1 I2S WDM skeleton, plus the resource-mapping
    and HAL bring-up performed on IRP_MN_START_DEVICE.

--*/

#include "driver.h"

static IO_COMPLETION_ROUTINE Rp1I2sSignalCompletion;

static NTSTATUS
Rp1I2sSignalCompletion(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp,
    _In_reads_opt_(_Inexpressible_("varies")) PVOID Context
    )
{
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);
    KeSetEvent((PKEVENT)Context, IO_NO_INCREMENT, FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

/* Send the IRP down and wait for the lower stack to finish (used for START). */
static NTSTATUS
Rp1I2sForwardAndWait(
    _In_ PRP1I2S_DEVICE_EXTENSION Ext,
    _Inout_ PIRP Irp
    )
{
    KEVENT   event;
    NTSTATUS status;

    KeInitializeEvent(&event, NotificationEvent, FALSE);
    IoCopyCurrentIrpStackLocationToNext(Irp);
    IoSetCompletionRoutine(Irp, Rp1I2sSignalCompletion, &event, TRUE, TRUE, TRUE);

    status = IoCallDriver(Ext->LowerDevice, Irp);
    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        status = Irp->IoStatus.Status;
    }
    return status;
}

NTSTATUS
Rp1I2sStartDevice(
    _Inout_ PRP1I2S_DEVICE_EXTENSION Ext,
    _In_opt_ PCM_RESOURCE_LIST Translated
    )
{
    ULONG                            i;
    PCM_PARTIAL_RESOURCE_LIST        list;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR  desc;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR  mem = NULL;

    /* No resources: allow the skeleton to load (e.g. root-enumerated test). */
    if (Translated == NULL || Translated->Count == 0) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_WARNING_LEVEL,
                   "rp1i2s: started with no hardware resources (skeleton mode)\n");
        return STATUS_SUCCESS;
    }

    list = &Translated->List[0].PartialResourceList;
    for (i = 0; i < list->Count; i++) {
        desc = &list->PartialDescriptors[i];
        if (desc->Type == CmResourceTypeMemory) {
            mem = desc;
            break;
        }
    }
    if (mem == NULL) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_WARNING_LEVEL,
                   "rp1i2s: no CmResourceTypeMemory in resource list\n");
        return STATUS_SUCCESS;
    }

    Ext->MmioPhys   = mem->u.Memory.Start;
    Ext->MmioLength = mem->u.Memory.Length;
    Ext->MmioBase   = (PUCHAR)MmMapIoSpaceEx(mem->u.Memory.Start,
                                             mem->u.Memory.Length,
                                             PAGE_READWRITE | PAGE_NOCACHE);
    if (Ext->MmioBase == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* ---- ported HAL bring-up (mirrors the Linux probe -> hw_params path) ---- */
    Rp1I2sHwInit(&Ext->Hw, Ext->MmioBase, Ext->MmioLength);
    (VOID)Rp1I2sHwProbe(&Ext->Hw);

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
        "rp1i2s: MMIO@0x%llx len=0x%Ix comp1=0x%08x fifo=%u cap=0x%x tx=%u rx=%u\n",
        (ULONGLONG)Ext->MmioPhys.QuadPart, Ext->MmioLength, Ext->Hw.Comp1,
        Ext->Hw.FifoDepth, Ext->Hw.Capability, Ext->Hw.TxChannelsMax,
        Ext->Hw.RxChannelsMax);

    /* Demonstrate the ported configure path: playback, stereo, 16-bit. */
    (VOID)Rp1I2sHwSetResolution(&Ext->Hw, 16);
    Rp1I2sHwFlush(&Ext->Hw, TRUE);
    Rp1I2sHwConfig(&Ext->Hw, TRUE, 2);
    Ext->Started = TRUE;

    return STATUS_SUCCESS;
}

VOID
Rp1I2sStopDevice(
    _Inout_ PRP1I2S_DEVICE_EXTENSION Ext
    )
{
    if (Ext->Started && Ext->MmioBase != NULL) {
        Rp1I2sHwStop(&Ext->Hw, TRUE);
        Ext->Started = FALSE;
    }
    if (Ext->MmioBase != NULL) {
        MmUnmapIoSpace(Ext->MmioBase, Ext->MmioLength);
        Ext->MmioBase = NULL;
    }
}

NTSTATUS
Rp1I2sDispatchPnp(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
    )
{
    PRP1I2S_DEVICE_EXTENSION ext = (PRP1I2S_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION       sl  = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS                 status;

    status = IoAcquireRemoveLock(&ext->RemoveLock, Irp);
    if (!NT_SUCCESS(status)) {
        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }

    switch (sl->MinorFunction) {

    case IRP_MN_START_DEVICE:
        /* Lower stack must start first, then we map our resources. */
        status = Rp1I2sForwardAndWait(ext, Irp);
        if (NT_SUCCESS(status)) {
            status = Rp1I2sStartDevice(
                        ext,
                        sl->Parameters.StartDevice.AllocatedResourcesTranslated);
        }
        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        IoReleaseRemoveLock(&ext->RemoveLock, Irp);
        return status;

    case IRP_MN_REMOVE_DEVICE:
        IoReleaseRemoveLockAndWait(&ext->RemoveLock, Irp);
        Rp1I2sStopDevice(ext);
        IoSkipCurrentIrpStackLocation(Irp);
        status = IoCallDriver(ext->LowerDevice, Irp);
        IoDetachDevice(ext->LowerDevice);
        IoDeleteDevice(ext->Self);
        return status;

    case IRP_MN_SURPRISE_REMOVAL:
        Rp1I2sStopDevice(ext);
        IoSkipCurrentIrpStackLocation(Irp);
        status = IoCallDriver(ext->LowerDevice, Irp);
        IoReleaseRemoveLock(&ext->RemoveLock, Irp);
        return status;

    default:
        IoSkipCurrentIrpStackLocation(Irp);
        status = IoCallDriver(ext->LowerDevice, Irp);
        IoReleaseRemoveLock(&ext->RemoveLock, Irp);
        return status;
    }
}

NTSTATUS
Rp1I2sDispatchPower(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
    )
{
    PRP1I2S_DEVICE_EXTENSION ext = (PRP1I2S_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    NTSTATUS                 status;

    status = IoAcquireRemoveLock(&ext->RemoveLock, Irp);
    if (!NT_SUCCESS(status)) {
        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }

    IoSkipCurrentIrpStackLocation(Irp);
    status = PoCallDriver(ext->LowerDevice, Irp);
    IoReleaseRemoveLock(&ext->RemoveLock, Irp);
    return status;
}

NTSTATUS
Rp1I2sDispatchSystemControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
    )
{
    PRP1I2S_DEVICE_EXTENSION ext = (PRP1I2S_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(ext->LowerDevice, Irp);
}
