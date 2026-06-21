/*++
Module Name: driver.c
Abstract:    DriverEntry + GpioClx client registration for the RP1 GPIO driver.

    Scaffold: the GpioClx client callbacks are forward-declared here via their
    WDK function typedefs (which pin the exact signatures); implementations live
    in ddi.c (next stage). DriverEntry registers the registration packet with
    GpioClx; AddDevice bridges WDF device creation to GpioClx.
--*/
#include "common.h"

DRIVER_INITIALIZE DriverEntry;

// ---- mandatory GpioClx client callbacks (signatures from gpioclx.h typedefs) ----
GPIO_CLIENT_PREPARE_CONTROLLER                  Rp1GpioPrepareController;
GPIO_CLIENT_RELEASE_CONTROLLER                  Rp1GpioReleaseController;
GPIO_CLIENT_START_CONTROLLER                    Rp1GpioStartController;
GPIO_CLIENT_STOP_CONTROLLER                     Rp1GpioStopController;
GPIO_CLIENT_QUERY_CONTROLLER_BASIC_INFORMATION  Rp1GpioQueryControllerBasicInformation;
GPIO_CLIENT_QUERY_SET_CONTROLLER_INFORMATION    Rp1GpioQuerySetControllerInformation;
GPIO_CLIENT_ENABLE_INTERRUPT                    Rp1GpioEnableInterrupt;
GPIO_CLIENT_DISABLE_INTERRUPT                   Rp1GpioDisableInterrupt;
GPIO_CLIENT_UNMASK_INTERRUPT                    Rp1GpioUnmaskInterrupt;
GPIO_CLIENT_MASK_INTERRUPTS                     Rp1GpioMaskInterrupts;
GPIO_CLIENT_QUERY_ACTIVE_INTERRUPTS            Rp1GpioQueryActiveInterrupts;
GPIO_CLIENT_CLEAR_ACTIVE_INTERRUPTS            Rp1GpioClearActiveInterrupts;
GPIO_CLIENT_CONNECT_IO_PINS                    Rp1GpioConnectIoPins;
GPIO_CLIENT_DISCONNECT_IO_PINS                 Rp1GpioDisconnectIoPins;
GPIO_CLIENT_READ_PINS                          Rp1GpioReadGpioPins;
GPIO_CLIENT_WRITE_PINS                         Rp1GpioWriteGpioPins;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#endif

NTSTATUS
Rp1GpioEvtDeviceAdd(_In_ WDFDRIVER Driver, _Inout_ PWDFDEVICE_INIT DeviceInit)
{
    NTSTATUS              status;
    WDF_OBJECT_ATTRIBUTES fdoAttributes;
    WDFDEVICE             device;

    WDF_OBJECT_ATTRIBUTES_INIT(&fdoAttributes);

    // GpioClx pre-create hook (adds its own device-init settings).
    status = GPIO_CLX_ProcessAddDevicePreDeviceCreate(Driver, DeviceInit, &fdoAttributes);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = WdfDeviceCreate(&DeviceInit, &fdoAttributes, &device);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // GpioClx post-create hook.
    return GPIO_CLX_ProcessAddDevicePostDeviceCreate(Driver, device);
}

NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG               config;
    WDFDRIVER                       driver;
    GPIO_CLIENT_REGISTRATION_PACKET pkt;
    NTSTATUS                        status;

    WDF_DRIVER_CONFIG_INIT(&config, Rp1GpioEvtDeviceAdd);
    config.DriverPoolTag = RP1GPIO_POOLTAG;

    status = WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES,
                             &config, &driver);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    RtlZeroMemory(&pkt, sizeof(pkt));
    pkt.Version               = GPIO_CLIENT_VERSION;
    pkt.Size                  = sizeof(GPIO_CLIENT_REGISTRATION_PACKET);
    pkt.ControllerContextSize = sizeof(RP1GPIO_CONTEXT);

    pkt.CLIENT_PrepareController                 = Rp1GpioPrepareController;
    pkt.CLIENT_ReleaseController                 = Rp1GpioReleaseController;
    pkt.CLIENT_StartController                   = Rp1GpioStartController;
    pkt.CLIENT_StopController                    = Rp1GpioStopController;
    pkt.CLIENT_QueryControllerBasicInformation   = Rp1GpioQueryControllerBasicInformation;
    pkt.CLIENT_QuerySetControllerInformation     = Rp1GpioQuerySetControllerInformation;
    pkt.CLIENT_EnableInterrupt                   = Rp1GpioEnableInterrupt;
    pkt.CLIENT_DisableInterrupt                  = Rp1GpioDisableInterrupt;
    pkt.CLIENT_UnmaskInterrupt                   = Rp1GpioUnmaskInterrupt;
    pkt.CLIENT_MaskInterrupts                    = Rp1GpioMaskInterrupts;
    pkt.CLIENT_QueryActiveInterrupts             = Rp1GpioQueryActiveInterrupts;
    pkt.CLIENT_ClearActiveInterrupts             = Rp1GpioClearActiveInterrupts;
    pkt.CLIENT_ConnectIoPins                     = Rp1GpioConnectIoPins;
    pkt.CLIENT_DisconnectIoPins                  = Rp1GpioDisconnectIoPins;
    pkt.CLIENT_ReadGpioPins                      = Rp1GpioReadGpioPins;
    pkt.CLIENT_WriteGpioPins                     = Rp1GpioWriteGpioPins;

    return GPIO_CLX_RegisterClient(driver, &pkt, RegistryPath);
}
