/*++
Module Name: driver.c
Abstract:    DriverEntry + GpioClx client registration for the BCM2712 GPIO
             driver (same structure as RP1 GPIO #9; brcmstb GIO HAL).
--*/
#include "common.h"

DRIVER_INITIALIZE DriverEntry;

GPIO_CLIENT_PREPARE_CONTROLLER                  BcmGpioPrepareController;
GPIO_CLIENT_RELEASE_CONTROLLER                  BcmGpioReleaseController;
GPIO_CLIENT_START_CONTROLLER                    BcmGpioStartController;
GPIO_CLIENT_STOP_CONTROLLER                     BcmGpioStopController;
GPIO_CLIENT_QUERY_CONTROLLER_BASIC_INFORMATION  BcmGpioQueryControllerBasicInformation;
GPIO_CLIENT_QUERY_SET_CONTROLLER_INFORMATION    BcmGpioQuerySetControllerInformation;
GPIO_CLIENT_ENABLE_INTERRUPT                    BcmGpioEnableInterrupt;
GPIO_CLIENT_DISABLE_INTERRUPT                   BcmGpioDisableInterrupt;
GPIO_CLIENT_UNMASK_INTERRUPT                    BcmGpioUnmaskInterrupt;
GPIO_CLIENT_MASK_INTERRUPTS                     BcmGpioMaskInterrupts;
GPIO_CLIENT_QUERY_ACTIVE_INTERRUPTS            BcmGpioQueryActiveInterrupts;
GPIO_CLIENT_CLEAR_ACTIVE_INTERRUPTS            BcmGpioClearActiveInterrupts;
GPIO_CLIENT_CONNECT_IO_PINS                    BcmGpioConnectIoPins;
GPIO_CLIENT_DISCONNECT_IO_PINS                 BcmGpioDisconnectIoPins;
GPIO_CLIENT_READ_PINS                          BcmGpioReadGpioPins;
GPIO_CLIENT_WRITE_PINS                         BcmGpioWriteGpioPins;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#endif

NTSTATUS
BcmGpioEvtDeviceAdd(_In_ WDFDRIVER Driver, _Inout_ PWDFDEVICE_INIT DeviceInit)
{
    NTSTATUS              status;
    WDF_OBJECT_ATTRIBUTES fdoAttributes;
    WDFDEVICE             device;

    WDF_OBJECT_ATTRIBUTES_INIT(&fdoAttributes);
    status = GPIO_CLX_ProcessAddDevicePreDeviceCreate(Driver, DeviceInit, &fdoAttributes);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = WdfDeviceCreate(&DeviceInit, &fdoAttributes, &device);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    return GPIO_CLX_ProcessAddDevicePostDeviceCreate(Driver, device);
}

NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG               config;
    WDFDRIVER                       driver;
    GPIO_CLIENT_REGISTRATION_PACKET pkt;
    NTSTATUS                        status;

    WDF_DRIVER_CONFIG_INIT(&config, BcmGpioEvtDeviceAdd);
    config.DriverPoolTag = BCMGPIO_POOLTAG;
    status = WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, &driver);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    RtlZeroMemory(&pkt, sizeof(pkt));
    pkt.Version               = GPIO_CLIENT_VERSION;
    pkt.Size                  = sizeof(GPIO_CLIENT_REGISTRATION_PACKET);
    pkt.ControllerContextSize = sizeof(BCMGPIO_CONTEXT);
    pkt.CLIENT_PrepareController               = BcmGpioPrepareController;
    pkt.CLIENT_ReleaseController               = BcmGpioReleaseController;
    pkt.CLIENT_StartController                 = BcmGpioStartController;
    pkt.CLIENT_StopController                  = BcmGpioStopController;
    pkt.CLIENT_QueryControllerBasicInformation = BcmGpioQueryControllerBasicInformation;
    pkt.CLIENT_QuerySetControllerInformation   = BcmGpioQuerySetControllerInformation;
    pkt.CLIENT_EnableInterrupt                 = BcmGpioEnableInterrupt;
    pkt.CLIENT_DisableInterrupt                = BcmGpioDisableInterrupt;
    pkt.CLIENT_UnmaskInterrupt                 = BcmGpioUnmaskInterrupt;
    pkt.CLIENT_MaskInterrupts                  = BcmGpioMaskInterrupts;
    pkt.CLIENT_QueryActiveInterrupts           = BcmGpioQueryActiveInterrupts;
    pkt.CLIENT_ClearActiveInterrupts           = BcmGpioClearActiveInterrupts;
    pkt.CLIENT_ConnectIoPins                   = BcmGpioConnectIoPins;
    pkt.CLIENT_DisconnectIoPins                = BcmGpioDisconnectIoPins;
    pkt.CLIENT_ReadGpioPins                    = BcmGpioReadGpioPins;
    pkt.CLIENT_WriteGpioPins                   = BcmGpioWriteGpioPins;

    return GPIO_CLX_RegisterClient(driver, &pkt, RegistryPath);
}
