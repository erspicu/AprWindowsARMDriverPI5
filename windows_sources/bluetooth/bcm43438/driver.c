/*++
Module Name: driver.c
Abstract:    KMDF DriverEntry + AddDevice for the BCM43438 BT UART HCI transport.
--*/
#include "common.h"

DRIVER_INITIALIZE DriverEntry;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#endif

NTSTATUS
BtBcmEvtDeviceAdd(_In_ WDFDRIVER Driver, _Inout_ PWDFDEVICE_INIT DeviceInit)
{
    NTSTATUS              status;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDFDEVICE             device;
    PBTBCM_CONTEXT        ctx;

    UNREFERENCED_PARAMETER(Driver);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, BTBCM_CONTEXT);

    status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    ctx = BtBcmGetContext(device);
    ctx->Device   = device;
    ctx->UartTarget = NULL;
    ctx->InitBaud = 115200;     /* BCM43438 boots at 115200 8N1 */
    ctx->OperBaud = 3000000;    /* operational 3 Mbps */

    /* TODO: open the underlying PL011 UART as a WDFIOTARGET (H4 HCI),
       run the BCM43438 bring-up sequence (BtBcmInitStepCount steps), then
       register as an HCI transport with the Windows BT stack (needs bthx.h). */
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
