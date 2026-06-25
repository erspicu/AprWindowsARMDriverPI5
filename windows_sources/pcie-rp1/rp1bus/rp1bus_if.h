/*++
Module Name: rp1bus_if.h
Abstract:    Public bus interface exported by rp1bus.sys to its child PDOs.
             A per-peripheral class driver (RP1 GPIO/I2C/SPI/UART/...) calls
             WdfFdoQueryForInterface(GUID_RP1BUS_INTERFACE_STANDARD) in its own
             EvtDevicePrepareHardware to obtain its slice of RP1 BAR1 (already
             mapped by the bus driver) + its RP1-internal interrupt index —
             without having to map MMIO or know the PCIe topology itself.

             This is the same pattern used by the WiFi sdbus_glue (query the
             parent for a hardware interface). Used instead of manufacturing
             CM_RESOURCE_LISTs because RP1's internal peripherals share one
             BAR1 window that only the bus driver maps, and the interrupts are
             demultiplexed by RP1's internal interrupt controller (a GpioClx-
             style role) rather than surfaced as discrete GSIs.

             Ship this header alongside the child driver sources.
--*/
#pragma once

#include <ntddk.h>

// {C7E9A1B2-4D3F-4A21-9B6E-2F1A7C0D5E88}
DEFINE_GUID(GUID_RP1BUS_INTERFACE_STANDARD,
    0xc7e9a1b2, 0x4d3f, 0x4a21, 0x9b, 0x6e, 0x2f, 0x1a, 0x7c, 0x0d, 0x5e, 0x88);

typedef struct _RP1BUS_INTERFACE_STANDARD {
    INTERFACE InterfaceHeader;   // Size/Version/Context/Reference/Dereference

    // Mapped (kernel VA) base of THIS child's register window inside BAR1.
    PVOID            (*GetRegisterBase)(_In_ PVOID Context);
    // Size of this child's register window, in bytes.
    ULONG            (*GetRegisterSize)(_In_ PVOID Context);
    // RP1-internal interrupt index for this child (feeds the RP1 IC / GpioClx).
    ULONG            (*GetInterruptIndex)(_In_ PVOID Context);
    // Physical base of this child's window (BAR1 phys + offset) — for DMA/debug.
    PHYSICAL_ADDRESS (*GetPhysicalBase)(_In_ PVOID Context);

} RP1BUS_INTERFACE_STANDARD, *PRP1BUS_INTERFACE_STANDARD;
