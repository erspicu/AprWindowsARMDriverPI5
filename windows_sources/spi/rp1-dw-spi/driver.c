/*++
Module Name: driver.c
Abstract:    SpbCx controller driver for the RP1 DesignWare SPI (DW-SSI).
             Mirrors the RP1 I2C SpbCx driver; uses the DW-SSI HAL for
             full-duplex polled transfers.
--*/
#include "common.h"

DRIVER_INITIALIZE DriverEntry;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, Rp1SpiEvtDeviceAdd)
#endif

NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, Rp1SpiEvtDeviceAdd);
    return WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES,
                           &config, WDF_NO_HANDLE);
}

NTSTATUS
Rp1SpiEvtDeviceAdd(_In_ WDFDRIVER Driver, _Inout_ PWDFDEVICE_INIT DeviceInit)
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
    pnp.EvtDevicePrepareHardware = Rp1SpiEvtPrepareHardware;
    pnp.EvtDeviceReleaseHardware = Rp1SpiEvtReleaseHardware;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnp);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attribs, RP1SPI_DEVICE);
    status = WdfDeviceCreate(&DeviceInit, &attribs, &device);
    if (!NT_SUCCESS(status)) return status;

    SPB_CONTROLLER_CONFIG_INIT(&spbConfig);
    spbConfig.EvtSpbTargetConnect    = Rp1SpiEvtTargetConnect;
    spbConfig.EvtSpbTargetDisconnect = Rp1SpiEvtTargetDisconnect;
    spbConfig.EvtSpbIoRead           = Rp1SpiEvtIoRead;
    spbConfig.EvtSpbIoWrite          = Rp1SpiEvtIoWrite;
    spbConfig.EvtSpbIoSequence       = Rp1SpiEvtIoSequence;
    return SpbDeviceInitialize(device, &spbConfig);
}

NTSTATUS
Rp1SpiEvtPrepareHardware(_In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesRaw, _In_ WDFCMRESLIST ResourcesTranslated)
{
    PRP1SPI_DEVICE ctx = Rp1SpiGetContext(Device);
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

    DwSpiHwInit(&ctx->Hw, ctx->Mmio, ctx->MmioLen);
    (VOID)DwSpiHwProbe(&ctx->Hw);
    DwSpiHwConfigureMaster(&ctx->Hw, 8, 0, 16);   /* 8-bit, mode 0, default div */
    return STATUS_SUCCESS;
}

NTSTATUS
Rp1SpiEvtReleaseHardware(_In_ WDFDEVICE Device, _In_ WDFCMRESLIST ResourcesTranslated)
{
    PRP1SPI_DEVICE ctx = Rp1SpiGetContext(Device);
    UNREFERENCED_PARAMETER(ResourcesTranslated);
    if (ctx->Mmio != NULL) {
        DwSpiHwEnable(&ctx->Hw, FALSE);
        MmUnmapIoSpace(ctx->Mmio, ctx->MmioLen);
        ctx->Mmio = NULL;
    }
    return STATUS_SUCCESS;
}

NTSTATUS
Rp1SpiEvtTargetConnect(_In_ WDFDEVICE Controller, _In_ SPBTARGET Target)
{
    PRP1SPI_DEVICE            ctx = Rp1SpiGetContext(Controller);
    SPB_CONNECTION_PARAMETERS params;

    RtlZeroMemory(&params, sizeof(params));
    SpbTargetGetConnectionParameters(Target, &params);
    // TODO: parse SPI mode / data-bit-length / connection speed / CS from the
    // ACPI SPI serial-bus descriptor. For now select CS0.
    DwSpiHwSetCs(&ctx->Hw, 0, TRUE);
    return STATUS_SUCCESS;
}

VOID
Rp1SpiEvtTargetDisconnect(_In_ WDFDEVICE Controller, _In_ SPBTARGET Target)
{
    PRP1SPI_DEVICE ctx = Rp1SpiGetContext(Controller);
    UNREFERENCED_PARAMETER(Target);
    DwSpiHwSetCs(&ctx->Hw, 0, FALSE);
}

VOID
Rp1SpiEvtIoWrite(_In_ WDFDEVICE Controller, _In_ SPBTARGET Target,
    _In_ SPBREQUEST Request, _In_ size_t Length)
{
    PRP1SPI_DEVICE ctx = Rp1SpiGetContext(Controller);
    PVOID buf = NULL; size_t bufLen = 0; NTSTATUS status;
    UNREFERENCED_PARAMETER(Target);
    status = WdfRequestRetrieveInputBuffer((WDFREQUEST)Request, Length, &buf, &bufLen);
    if (NT_SUCCESS(status)) {
        status = DwSpiHwTransferPolled(&ctx->Hw, (const UCHAR *)buf, NULL, (ULONG)bufLen);
    }
    SpbRequestComplete(Request, status);
}

VOID
Rp1SpiEvtIoRead(_In_ WDFDEVICE Controller, _In_ SPBTARGET Target,
    _In_ SPBREQUEST Request, _In_ size_t Length)
{
    PRP1SPI_DEVICE ctx = Rp1SpiGetContext(Controller);
    PVOID buf = NULL; size_t bufLen = 0; NTSTATUS status;
    UNREFERENCED_PARAMETER(Target);
    status = WdfRequestRetrieveOutputBuffer((WDFREQUEST)Request, Length, &buf, &bufLen);
    if (NT_SUCCESS(status)) {
        status = DwSpiHwTransferPolled(&ctx->Hw, NULL, (UCHAR *)buf, (ULONG)bufLen);
        if (NT_SUCCESS(status)) WdfRequestSetInformation((WDFREQUEST)Request, bufLen);
    }
    SpbRequestComplete(Request, status);
}

VOID
Rp1SpiEvtIoSequence(_In_ WDFDEVICE Controller, _In_ SPBTARGET Target,
    _In_ SPBREQUEST Request, _In_ ULONG TransferCount)
{
    UNREFERENCED_PARAMETER(Controller);
    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(TransferCount);
    // TODO: full-duplex write+read via the SPB_TRANSFER_LIST.
    SpbRequestComplete(Request, STATUS_NOT_SUPPORTED);
}
