/*++
Module Name: driver.c
Abstract:    SpbCx controller driver for the BCM2712 BSC I2C. Same structure as
             the RP1 I2C SpbCx driver; uses the BSC HAL.
--*/
#include "common.h"

DRIVER_INITIALIZE DriverEntry;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, BcmI2cEvtDeviceAdd)
#endif

NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, BcmI2cEvtDeviceAdd);
    return WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES,
                           &config, WDF_NO_HANDLE);
}

NTSTATUS
BcmI2cEvtDeviceAdd(_In_ WDFDRIVER Driver, _Inout_ PWDFDEVICE_INIT DeviceInit)
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
    pnp.EvtDevicePrepareHardware = BcmI2cEvtPrepareHardware;
    pnp.EvtDeviceReleaseHardware = BcmI2cEvtReleaseHardware;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnp);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attribs, BCMI2C_DEVICE);
    status = WdfDeviceCreate(&DeviceInit, &attribs, &device);
    if (!NT_SUCCESS(status)) return status;

    SPB_CONTROLLER_CONFIG_INIT(&spbConfig);
    spbConfig.EvtSpbTargetConnect    = BcmI2cEvtTargetConnect;
    spbConfig.EvtSpbTargetDisconnect = BcmI2cEvtTargetDisconnect;
    spbConfig.EvtSpbIoRead           = BcmI2cEvtIoRead;
    spbConfig.EvtSpbIoWrite          = BcmI2cEvtIoWrite;
    spbConfig.EvtSpbIoSequence       = BcmI2cEvtIoSequence;
    return SpbDeviceInitialize(device, &spbConfig);
}

NTSTATUS
BcmI2cEvtPrepareHardware(_In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesRaw, _In_ WDFCMRESLIST ResourcesTranslated)
{
    PBCMI2C_DEVICE ctx = BcmI2cGetContext(Device);
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

    BcmI2cHwInit(&ctx->Hw, ctx->Mmio, ctx->MmioLen);
    BcmI2cHwSetDivider(&ctx->Hw, 0);   /* default ~100 kHz */
    return STATUS_SUCCESS;
}

NTSTATUS
BcmI2cEvtReleaseHardware(_In_ WDFDEVICE Device, _In_ WDFCMRESLIST ResourcesTranslated)
{
    PBCMI2C_DEVICE ctx = BcmI2cGetContext(Device);
    UNREFERENCED_PARAMETER(ResourcesTranslated);
    if (ctx->Mmio != NULL) {
        MmUnmapIoSpace(ctx->Mmio, ctx->MmioLen);
        ctx->Mmio = NULL;
    }
    return STATUS_SUCCESS;
}

NTSTATUS
BcmI2cEvtTargetConnect(_In_ WDFDEVICE Controller, _In_ SPBTARGET Target)
{
    PBCMI2C_DEVICE            ctx = BcmI2cGetContext(Controller);
    SPB_CONNECTION_PARAMETERS params;

    RtlZeroMemory(&params, sizeof(params));
    SpbTargetGetConnectionParameters(Target, &params);
    // TODO: parse the I2C slave address from the ACPI descriptor.
    BcmI2cHwSetTarget(&ctx->Hw, 0x00);
    return STATUS_SUCCESS;
}

VOID
BcmI2cEvtTargetDisconnect(_In_ WDFDEVICE Controller, _In_ SPBTARGET Target)
{
    UNREFERENCED_PARAMETER(Controller);
    UNREFERENCED_PARAMETER(Target);
}

VOID
BcmI2cEvtIoWrite(_In_ WDFDEVICE Controller, _In_ SPBTARGET Target,
    _In_ SPBREQUEST Request, _In_ size_t Length)
{
    PBCMI2C_DEVICE ctx = BcmI2cGetContext(Controller);
    PVOID buf = NULL; size_t bufLen = 0; NTSTATUS status;
    UNREFERENCED_PARAMETER(Target);
    status = WdfRequestRetrieveInputBuffer((WDFREQUEST)Request, Length, &buf, &bufLen);
    if (NT_SUCCESS(status)) {
        status = BcmI2cHwWritePolled(&ctx->Hw, (const UCHAR *)buf, (ULONG)bufLen);
    }
    SpbRequestComplete(Request, status);
}

VOID
BcmI2cEvtIoRead(_In_ WDFDEVICE Controller, _In_ SPBTARGET Target,
    _In_ SPBREQUEST Request, _In_ size_t Length)
{
    PBCMI2C_DEVICE ctx = BcmI2cGetContext(Controller);
    PVOID buf = NULL; size_t bufLen = 0; NTSTATUS status;
    UNREFERENCED_PARAMETER(Target);
    status = WdfRequestRetrieveOutputBuffer((WDFREQUEST)Request, Length, &buf, &bufLen);
    if (NT_SUCCESS(status)) {
        status = BcmI2cHwReadPolled(&ctx->Hw, (UCHAR *)buf, (ULONG)bufLen);
        if (NT_SUCCESS(status)) WdfRequestSetInformation((WDFREQUEST)Request, bufLen);
    }
    SpbRequestComplete(Request, status);
}

VOID
BcmI2cEvtIoSequence(_In_ WDFDEVICE Controller, _In_ SPBTARGET Target,
    _In_ SPBREQUEST Request, _In_ ULONG TransferCount)
{
    UNREFERENCED_PARAMETER(Controller);
    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(TransferCount);
    SpbRequestComplete(Request, STATUS_NOT_SUPPORTED);
}
