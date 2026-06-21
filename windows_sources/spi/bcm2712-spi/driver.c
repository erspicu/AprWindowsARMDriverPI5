/*++
Module Name: driver.c
Abstract:    SpbCx controller driver for the BCM2712 SPI. Same structure as the
             RP1 SPI SpbCx driver; uses the BCM SPI HAL.
--*/
#include "common.h"

DRIVER_INITIALIZE DriverEntry;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, BcmSpiEvtDeviceAdd)
#endif

NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, BcmSpiEvtDeviceAdd);
    return WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES,
                           &config, WDF_NO_HANDLE);
}

NTSTATUS
BcmSpiEvtDeviceAdd(_In_ WDFDRIVER Driver, _Inout_ PWDFDEVICE_INIT DeviceInit)
{
    NTSTATUS                     status;
    WDF_PNPPOWER_EVENT_CALLBACKS pnp;
    WDF_OBJECT_ATTRIBUTES        attribs;
    WDFDEVICE                    device;
    SPB_CONTROLLER_CONFIG        spbConfig;

    UNREFERENCED_PARAMETER(Driver);
    PAGED_CODE();

    status = SpbDeviceInitConfig(DeviceInit);
    if (!NT_SUCCESS(status)) return status;

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
    pnp.EvtDevicePrepareHardware = BcmSpiEvtPrepareHardware;
    pnp.EvtDeviceReleaseHardware = BcmSpiEvtReleaseHardware;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnp);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attribs, BCMSPI_DEVICE);
    status = WdfDeviceCreate(&DeviceInit, &attribs, &device);
    if (!NT_SUCCESS(status)) return status;

    SPB_CONTROLLER_CONFIG_INIT(&spbConfig);
    spbConfig.EvtSpbTargetConnect    = BcmSpiEvtTargetConnect;
    spbConfig.EvtSpbTargetDisconnect = BcmSpiEvtTargetDisconnect;
    spbConfig.EvtSpbIoRead           = BcmSpiEvtIoRead;
    spbConfig.EvtSpbIoWrite          = BcmSpiEvtIoWrite;
    spbConfig.EvtSpbIoSequence       = BcmSpiEvtIoSequence;
    return SpbDeviceInitialize(device, &spbConfig);
}

NTSTATUS
BcmSpiEvtPrepareHardware(_In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesRaw, _In_ WDFCMRESLIST ResourcesTranslated)
{
    PBCMSPI_DEVICE ctx = BcmSpiGetContext(Device);
    ULONG          count = WdfCmResourceListGetCount(ResourcesTranslated);
    PCM_PARTIAL_RESOURCE_DESCRIPTOR mem = NULL;
    ULONG          i;

    UNREFERENCED_PARAMETER(ResourcesRaw);
    for (i = 0; i < count; i++) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR d = WdfCmResourceListGetDescriptor(ResourcesTranslated, i);
        if (d != NULL && d->Type == CmResourceTypeMemory) { mem = d; break; }
    }
    if (mem == NULL) return STATUS_DEVICE_CONFIGURATION_ERROR;

    ctx->MmioLen = mem->u.Memory.Length;
    ctx->Mmio    = (PUCHAR)MmMapIoSpaceEx(mem->u.Memory.Start, mem->u.Memory.Length,
                                          PAGE_READWRITE | PAGE_NOCACHE);
    if (ctx->Mmio == NULL) return STATUS_INSUFFICIENT_RESOURCES;

    BcmSpiHwInit(&ctx->Hw, ctx->Mmio, ctx->MmioLen);
    BcmSpiHwSetClock(&ctx->Hw, 0x100);   /* explicit default ~2.93 MHz (0 would be slowest) */
    BcmSpiHwSetMode(&ctx->Hw, 0);    /* mode 0 */
    return STATUS_SUCCESS;
}

NTSTATUS
BcmSpiEvtReleaseHardware(_In_ WDFDEVICE Device, _In_ WDFCMRESLIST ResourcesTranslated)
{
    PBCMSPI_DEVICE ctx = BcmSpiGetContext(Device);
    UNREFERENCED_PARAMETER(ResourcesTranslated);
    if (ctx->Mmio != NULL) {
        MmUnmapIoSpace(ctx->Mmio, ctx->MmioLen);
        ctx->Mmio = NULL;
    }
    return STATUS_SUCCESS;
}

NTSTATUS
BcmSpiEvtTargetConnect(_In_ WDFDEVICE Controller, _In_ SPBTARGET Target)
{
    SPB_CONNECTION_PARAMETERS params;
    UNREFERENCED_PARAMETER(Controller);
    RtlZeroMemory(&params, sizeof(params));
    SpbTargetGetConnectionParameters(Target, &params);
    // TODO: parse SPI mode / speed from the ACPI descriptor.
    return STATUS_SUCCESS;
}

VOID
BcmSpiEvtTargetDisconnect(_In_ WDFDEVICE Controller, _In_ SPBTARGET Target)
{
    UNREFERENCED_PARAMETER(Controller);
    UNREFERENCED_PARAMETER(Target);
}

VOID
BcmSpiEvtIoWrite(_In_ WDFDEVICE Controller, _In_ SPBTARGET Target,
    _In_ SPBREQUEST Request, _In_ size_t Length)
{
    PBCMSPI_DEVICE ctx = BcmSpiGetContext(Controller);
    PVOID buf = NULL; size_t bufLen = 0; NTSTATUS status;
    UNREFERENCED_PARAMETER(Target);
    status = WdfRequestRetrieveInputBuffer((WDFREQUEST)Request, Length, &buf, &bufLen);
    if (NT_SUCCESS(status)) {
        status = BcmSpiHwTransferPolled(&ctx->Hw, (const UCHAR *)buf, NULL, (ULONG)bufLen);
    }
    SpbRequestComplete(Request, status);
}

VOID
BcmSpiEvtIoRead(_In_ WDFDEVICE Controller, _In_ SPBTARGET Target,
    _In_ SPBREQUEST Request, _In_ size_t Length)
{
    PBCMSPI_DEVICE ctx = BcmSpiGetContext(Controller);
    PVOID buf = NULL; size_t bufLen = 0; NTSTATUS status;
    UNREFERENCED_PARAMETER(Target);
    status = WdfRequestRetrieveOutputBuffer((WDFREQUEST)Request, Length, &buf, &bufLen);
    if (NT_SUCCESS(status)) {
        status = BcmSpiHwTransferPolled(&ctx->Hw, NULL, (UCHAR *)buf, (ULONG)bufLen);
        if (NT_SUCCESS(status)) WdfRequestSetInformation((WDFREQUEST)Request, bufLen);
    }
    SpbRequestComplete(Request, status);
}

VOID
BcmSpiEvtIoSequence(_In_ WDFDEVICE Controller, _In_ SPBTARGET Target,
    _In_ SPBREQUEST Request, _In_ ULONG TransferCount)
{
    UNREFERENCED_PARAMETER(Controller);
    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(TransferCount);
    SpbRequestComplete(Request, STATUS_NOT_SUPPORTED);
}
