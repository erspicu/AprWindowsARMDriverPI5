/*++ driver.c - KMDF entry for the Raspberry Pi firmware RTC (skeleton). --*/
#include "common.h"

DRIVER_INITIALIZE DriverEntry;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#endif

NTSTATUS
RpiRtcEvtDeviceAdd(_In_ WDFDRIVER Driver, _Inout_ PWDFDEVICE_INIT DeviceInit)
{
    WDF_OBJECT_ATTRIBUTES attr;
    WDFDEVICE             device;

    UNREFERENCED_PARAMETER(Driver);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr, RPIRTC_CONTEXT);
    /* No PrepareHardware: the RTC has no MMIO; time get/set is proxied through
       the VideoCore firmware property channel (mailbox driver #16). */
    return WdfDeviceCreate(&DeviceInit, &attr, &device);
}

NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, RpiRtcEvtDeviceAdd);
    config.DriverPoolTag = RPIRTC_TAG;
    return WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES,
                           &config, WDF_NO_HANDLE);
}
