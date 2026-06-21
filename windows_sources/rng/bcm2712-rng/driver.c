/*++ driver.c - KMDF entry for the BCM2712 RNG200 driver. --*/
#include "common.h"

DRIVER_INITIALIZE DriverEntry;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#endif

NTSTATUS
BcmRngEvtPrepareHardware(_In_ WDFDEVICE Device, _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated)
{
    PBCMRNG_CONTEXT ctx = BcmRngGetContext(Device);
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
    if (ctx->Base != NULL) {
        RngHwInit(ctx->Base);     /* enable the random bit generator */
    }
    return STATUS_SUCCESS;
}

NTSTATUS
BcmRngEvtReleaseHardware(_In_ WDFDEVICE Device, _In_ WDFCMRESLIST ResourcesTranslated)
{
    PBCMRNG_CONTEXT ctx = BcmRngGetContext(Device);
    UNREFERENCED_PARAMETER(ResourcesTranslated);
    if (ctx->Base != NULL) {
        RngHwDisable(ctx->Base);
        MmUnmapIoSpace(ctx->Base, ctx->Length);
        ctx->Base = NULL;
    }
    return STATUS_SUCCESS;
}

NTSTATUS
BcmRngEvtDeviceAdd(_In_ WDFDRIVER Driver, _Inout_ PWDFDEVICE_INIT DeviceInit)
{
    WDF_PNPPOWER_EVENT_CALLBACKS pnp;
    WDF_OBJECT_ATTRIBUTES        attr;
    WDFDEVICE                    device;

    UNREFERENCED_PARAMETER(Driver);
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
    pnp.EvtDevicePrepareHardware = BcmRngEvtPrepareHardware;
    pnp.EvtDeviceReleaseHardware = BcmRngEvtReleaseHardware;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnp);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr, BCMRNG_CONTEXT);
    return WdfDeviceCreate(&DeviceInit, &attr, &device);
}

NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, BcmRngEvtDeviceAdd);
    config.DriverPoolTag = BCMRNG_TAG;
    return WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES,
                           &config, WDF_NO_HANDLE);
}
