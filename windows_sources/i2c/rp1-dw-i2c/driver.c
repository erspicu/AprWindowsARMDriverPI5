/*++
Module Name: driver.c
Abstract:    SpbCx controller driver for the RP1 DesignWare I2C. Registers with
             SpbCx; services target connect + read/write via the ported HAL.
             (Sequence + full SPB transfer-list + interrupt mode are refinements.)
--*/
#include "common.h"

DRIVER_INITIALIZE DriverEntry;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, Rp1I2cEvtDeviceAdd)
#endif

NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, Rp1I2cEvtDeviceAdd);
    return WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES,
                           &config, WDF_NO_HANDLE);
}

NTSTATUS
Rp1I2cEvtDeviceAdd(_In_ WDFDRIVER Driver, _Inout_ PWDFDEVICE_INIT DeviceInit)
{
    NTSTATUS                     status;
    WDF_PNPPOWER_EVENT_CALLBACKS pnp;
    WDF_OBJECT_ATTRIBUTES        attribs;
    WDFDEVICE                    device;
    SPB_CONTROLLER_CONFIG        spbConfig;

    UNREFERENCED_PARAMETER(Driver);
    PAGED_CODE();

    // Tell the framework this is an SpbCx controller before creating the device.
    status = SpbDeviceInitConfig(DeviceInit);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
    pnp.EvtDevicePrepareHardware = Rp1I2cEvtPrepareHardware;
    pnp.EvtDeviceReleaseHardware = Rp1I2cEvtReleaseHardware;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnp);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attribs, RP1I2C_DEVICE);
    status = WdfDeviceCreate(&DeviceInit, &attribs, &device);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    SPB_CONTROLLER_CONFIG_INIT(&spbConfig);
    spbConfig.EvtSpbTargetConnect    = Rp1I2cEvtTargetConnect;
    spbConfig.EvtSpbTargetDisconnect = Rp1I2cEvtTargetDisconnect;
    spbConfig.EvtSpbIoRead           = Rp1I2cEvtIoRead;
    spbConfig.EvtSpbIoWrite          = Rp1I2cEvtIoWrite;
    spbConfig.EvtSpbIoSequence       = Rp1I2cEvtIoSequence;

    return SpbDeviceInitialize(device, &spbConfig);
}

NTSTATUS
Rp1I2cEvtPrepareHardware(_In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesRaw, _In_ WDFCMRESLIST ResourcesTranslated)
{
    PRP1I2C_DEVICE ctx = Rp1I2cGetContext(Device);
    ULONG          count = WdfCmResourceListGetCount(ResourcesTranslated);
    PCM_PARTIAL_RESOURCE_DESCRIPTOR mem = NULL;
    ULONG          i;

    UNREFERENCED_PARAMETER(ResourcesRaw);

    for (i = 0; i < count; i++) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR d = WdfCmResourceListGetDescriptor(ResourcesTranslated, i);
        if (d != NULL && d->Type == CmResourceTypeMemory) { mem = d; break; }
    }
    if (mem == NULL) {
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    ctx->MmioLen = mem->u.Memory.Length;
    ctx->Mmio    = (PUCHAR)MmMapIoSpaceEx(mem->u.Memory.Start, mem->u.Memory.Length,
                                          PAGE_READWRITE | PAGE_NOCACHE);
    if (ctx->Mmio == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    DwI2cHwInit(&ctx->Hw, ctx->Mmio, ctx->MmioLen);
    (VOID)DwI2cHwProbe(&ctx->Hw);
    DwI2cHwConfigureMaster(&ctx->Hw, DwI2cFast);   // 400 kHz default
    return STATUS_SUCCESS;
}

NTSTATUS
Rp1I2cEvtReleaseHardware(_In_ WDFDEVICE Device, _In_ WDFCMRESLIST ResourcesTranslated)
{
    PRP1I2C_DEVICE ctx = Rp1I2cGetContext(Device);
    UNREFERENCED_PARAMETER(ResourcesTranslated);
    if (ctx->Mmio != NULL) {
        DwI2cHwDisable(&ctx->Hw);
        MmUnmapIoSpace(ctx->Mmio, ctx->MmioLen);
        ctx->Mmio = NULL;
    }
    return STATUS_SUCCESS;
}

NTSTATUS
Rp1I2cEvtTargetConnect(_In_ WDFDEVICE Controller, _In_ SPBTARGET Target)
{
    PRP1I2C_DEVICE             ctx = Rp1I2cGetContext(Controller);
    SPB_CONNECTION_PARAMETERS  params;

    RtlZeroMemory(&params, sizeof(params));
    SpbTargetGetConnectionParameters(Target, &params);
    // TODO: parse the I2C slave address from the ACPI I2C serial-bus descriptor
    // in params.ConnectionParameters. For now program a placeholder so the
    // controller is armed; SetTarget re-enables the controller with TAR.
    DwI2cHwSetTarget(&ctx->Hw, 0x00);
    return STATUS_SUCCESS;
}

VOID
Rp1I2cEvtTargetDisconnect(_In_ WDFDEVICE Controller, _In_ SPBTARGET Target)
{
    UNREFERENCED_PARAMETER(Controller);
    UNREFERENCED_PARAMETER(Target);
}

VOID
Rp1I2cEvtIoWrite(_In_ WDFDEVICE Controller, _In_ SPBTARGET Target,
    _In_ SPBREQUEST Request, _In_ size_t Length)
{
    PRP1I2C_DEVICE ctx = Rp1I2cGetContext(Controller);
    PVOID          buf = NULL;
    size_t         bufLen = 0;
    NTSTATUS       status;

    UNREFERENCED_PARAMETER(Target);
    status = WdfRequestRetrieveInputBuffer((WDFREQUEST)Request, Length, &buf, &bufLen);
    if (NT_SUCCESS(status)) {
        status = DwI2cHwWritePolled(&ctx->Hw, (const UCHAR *)buf, (ULONG)bufLen, TRUE);
    }
    SpbRequestComplete(Request, status);
}

VOID
Rp1I2cEvtIoRead(_In_ WDFDEVICE Controller, _In_ SPBTARGET Target,
    _In_ SPBREQUEST Request, _In_ size_t Length)
{
    PRP1I2C_DEVICE ctx = Rp1I2cGetContext(Controller);
    PVOID          buf = NULL;
    size_t         bufLen = 0;
    NTSTATUS       status;

    UNREFERENCED_PARAMETER(Target);
    status = WdfRequestRetrieveOutputBuffer((WDFREQUEST)Request, Length, &buf, &bufLen);
    if (NT_SUCCESS(status)) {
        status = DwI2cHwReadPolled(&ctx->Hw, (UCHAR *)buf, (ULONG)bufLen, TRUE);
        if (NT_SUCCESS(status)) {
            WdfRequestSetInformation((WDFREQUEST)Request, bufLen);
        }
    }
    SpbRequestComplete(Request, status);
}

VOID
Rp1I2cEvtIoSequence(_In_ WDFDEVICE Controller, _In_ SPBTARGET Target,
    _In_ SPBREQUEST Request, _In_ ULONG TransferCount)
{
    UNREFERENCED_PARAMETER(Controller);
    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(TransferCount);
    // TODO: walk the SPB_TRANSFER_LIST (write-then-read combined transactions).
    SpbRequestComplete(Request, STATUS_NOT_SUPPORTED);
}
