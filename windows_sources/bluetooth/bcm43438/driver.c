/*++
Module Name: driver.c
Abstract:    KMDF DriverEntry + AddDevice for the BCM43438 BT UART HCI transport.
--*/
#include "common.h"

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DEVICE_PREPARE_HARDWARE BtBcmEvtPrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE BtBcmEvtReleaseHardware;
EVT_WDF_DEVICE_D0_ENTRY         BtBcmEvtD0Entry;
EVT_WDF_DEVICE_D0_EXIT          BtBcmEvtD0Exit;

#define BTBCM_HCD_PATH  L"\\SystemRoot\\System32\\drivers\\BCM4345C0.hcd"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, BtBcmEvtPrepareHardware)
#pragma alloc_text(PAGE, BtBcmEvtReleaseHardware)
#endif

/*
 * D0Entry (PASSIVE_LEVEL): load the BCM .hcd, run the bring-up state machine over
 * the UART, then start the operational RX pump. From S4/cold this reloads the
 * firmware; from a warm resume the chip may still hold it (a refinement TODO).
 */
NTSTATUS
BtBcmEvtD0Entry(_In_ WDFDEVICE Device, _In_ WDF_POWER_DEVICE_STATE PreviousState)
{
    PBTBCM_CONTEXT ctx = BtBcmGetContext(Device);
    NTSTATUS status;

    UNREFERENCED_PARAMETER(PreviousState);

    if (ctx->Hcd == NULL) {
        status = BtBcmLoadFirmware(ctx, BTBCM_HCD_PATH);
        if (!NT_SUCCESS(status)) {
            return status;          /* no firmware -> cannot bring the chip up */
        }
    }
    status = BtBcmBringUp(ctx, ctx->Hcd, ctx->HcdLen);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    return BtBcmStartRxPump(ctx);
}

NTSTATUS
BtBcmEvtD0Exit(_In_ WDFDEVICE Device, _In_ WDF_POWER_DEVICE_STATE TargetState)
{
    PBTBCM_CONTEXT ctx = BtBcmGetContext(Device);
    UNREFERENCED_PARAMETER(TargetState);
    ctx->Running = FALSE;                       /* stop the RX pump re-arming */
    if (ctx->HasGpio) {
        (void)BtBcmGpioWrite(ctx, FALSE);       /* drop BT_REG_ON (cold-exit) */
    }
    return STATUS_SUCCESS;
}

NTSTATUS
BtBcmEvtReleaseHardware(_In_ WDFDEVICE Device, _In_ WDFCMRESLIST ResourcesTranslated)
{
    PBTBCM_CONTEXT ctx = BtBcmGetContext(Device);
    PAGED_CODE();
    UNREFERENCED_PARAMETER(ResourcesTranslated);
    BtBcmFreeFirmware(ctx);
    return STATUS_SUCCESS;
}

/*
 * EvtDevicePrepareHardware (B1): parse the ACPI _CRS (UARTSerialBusV2 + GpioIo)
 * and open the UART + reset-GPIO as WDFIOTARGETs. The BCM firmware bring-up
 * (BtBcmBringUp, which needs the .hcd blob loaded + real hardware) is wired from
 * D0Entry in a later step; see PLAN.md Phase B/C.
 */
NTSTATUS
BtBcmEvtPrepareHardware(_In_ WDFDEVICE Device, _In_ WDFCMRESLIST ResourcesRaw,
                        _In_ WDFCMRESLIST ResourcesTranslated)
{
    PBTBCM_CONTEXT ctx = BtBcmGetContext(Device);
    NTSTATUS status;

    PAGED_CODE();
    UNREFERENCED_PARAMETER(ResourcesRaw);

    status = BtBcmParseResources(ctx, ResourcesTranslated);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    return BtBcmOpenTargets(ctx);
}

NTSTATUS
BtBcmEvtDeviceAdd(_In_ WDFDRIVER Driver, _Inout_ PWDFDEVICE_INIT DeviceInit)
{
    NTSTATUS                     status;
    WDF_OBJECT_ATTRIBUTES        attributes;
    WDF_PNPPOWER_EVENT_CALLBACKS pnp;
    WDFDEVICE                    device;
    PBTBCM_CONTEXT               ctx;

    UNREFERENCED_PARAMETER(Driver);

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
    pnp.EvtDevicePrepareHardware = BtBcmEvtPrepareHardware;
    pnp.EvtDeviceReleaseHardware = BtBcmEvtReleaseHardware;
    pnp.EvtDeviceD0Entry         = BtBcmEvtD0Entry;
    pnp.EvtDeviceD0Exit          = BtBcmEvtD0Exit;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnp);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, BTBCM_CONTEXT);

    status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    ctx = BtBcmGetContext(device);
    ctx->Device     = device;
    ctx->UartTarget = NULL;
    ctx->GpioTarget = NULL;
    ctx->InitBaud   = 115200;     /* BCM4345C0 boots at 115200 8N1 */
    ctx->OperBaud   = 3000000;    /* operational 3 Mbps (Pi5 DT max-speed) */

    /* Next (Phase B/C, needs Win11 target): load brcm/BCM4345C0.hcd, call
       BtBcmBringUp() from D0Entry, then present HCI via inbox BthUart.sys
       (INF Needs=BthUart.NT) or a thin BTHX/filter shim. See PLAN.md. */
    return STATUS_SUCCESS;
}

NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG config;

    WDF_DRIVER_CONFIG_INIT(&config, BtBcmEvtDeviceAdd);
    config.DriverPoolTag = BTBCM_POOLTAG;

    return WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES,
                           &config, WDF_NO_HANDLE);
}
