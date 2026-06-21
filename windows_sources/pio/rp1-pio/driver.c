/*++ driver.c - KMDF entry for the RP1 PIO driver. --*/
#include "common.h"

DRIVER_INITIALIZE DriverEntry;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#endif

NTSTATUS
Rp1PioEvtPrepareHardware(_In_ WDFDEVICE Device, _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated)
{
    PRP1PIO_CONTEXT ctx = Rp1PioGetContext(Device);
    ULONG i, count = WdfCmResourceListGetCount(ResourcesTranslated);

    UNREFERENCED_PARAMETER(ResourcesRaw);
    for (i = 0; i < count; i++) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR d = WdfCmResourceListGetDescriptor(ResourcesTranslated, i);
        if (d != NULL && d->Type == CmResourceTypeMemory) {
            ctx->Length = d->u.Memory.Length;
            ctx->Base   = MmMapIoSpaceEx(d->u.Memory.Start, d->u.Memory.Length,
                                         PAGE_READWRITE | PAGE_NOCACHE);
            break;
        }
    }
    return STATUS_SUCCESS;
}

NTSTATUS
Rp1PioEvtReleaseHardware(_In_ WDFDEVICE Device, _In_ WDFCMRESLIST ResourcesTranslated)
{
    PRP1PIO_CONTEXT ctx = Rp1PioGetContext(Device);
    UNREFERENCED_PARAMETER(ResourcesTranslated);
    if (ctx->Base != NULL) {
        MmUnmapIoSpace(ctx->Base, ctx->Length);
        ctx->Base = NULL;
    }
    return STATUS_SUCCESS;
}

NTSTATUS
Rp1PioEvtDeviceAdd(_In_ WDFDRIVER Driver, _Inout_ PWDFDEVICE_INIT DeviceInit)
{
    WDF_PNPPOWER_EVENT_CALLBACKS pnp;
    WDF_OBJECT_ATTRIBUTES        attr;
    WDFDEVICE                    device;

    UNREFERENCED_PARAMETER(Driver);
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
    pnp.EvtDevicePrepareHardware = Rp1PioEvtPrepareHardware;
    pnp.EvtDeviceReleaseHardware = Rp1PioEvtReleaseHardware;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnp);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr, RP1PIO_CONTEXT);
    return WdfDeviceCreate(&DeviceInit, &attr, &device);
}

NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, Rp1PioEvtDeviceAdd);
    config.DriverPoolTag = RP1PIO_TAG;
    return WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES,
                           &config, WDF_NO_HANDLE);
}
