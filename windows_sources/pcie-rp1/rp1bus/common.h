/*++
Module Name: common.h
Abstract:    RP1 PCIe multi-function bus driver (KMDF) - the foundational
             "make RP1 visible" piece. Binds to PCI\VEN_1DE4&DEV_0001, maps
             RP1 BAR1, and enumerates the internal peripherals (UART/I2C/SPI/
             GPIO/Ethernet/USB/I2S...) as child PDOs so per-peripheral class
             drivers can load. (See MD/Note/20260621-0415-...-prereq-design.md)
--*/
#pragma once

#include <ntddk.h>
#include <wdf.h>

#define RP1BUS_POOLTAG          'sbPR'   // 'RPbs'
#define RP1_PCI_VENDOR_ID       0x1DE4   // Raspberry Pi Ltd
#define RP1_PCI_DEVICE_ID       0x0001   // RP1 C0
#define RP1_APBS_IRQ_BASE       0x108000 // internal interrupt controller in BAR1
#define RP1_INT_COUNT           61       // internal IRQs (one MSI-X vector each)

// One RP1 internal peripheral: a slice of BAR1 + an internal IRQ index.
typedef struct _RP1_PERIPH {
    PCWSTR  HardwareId;     // e.g. L"RP1\\UART0"
    PCWSTR  InstanceId;     // unique per child
    ULONG   Offset;         // byte offset within BAR1
    ULONG   Size;           // register window size
    ULONG   Irq;            // RP1 internal IRQ index (== GpioInt pin)
} RP1_PERIPH;

typedef struct _RP1BUS_FDO_CONTEXT {
    PHYSICAL_ADDRESS BarPhys;     // BAR1 physical base (for slicing children)
    PVOID            BarBase;     // mapped BAR1
    SIZE_T           BarLength;
} RP1BUS_FDO_CONTEXT, *PRP1BUS_FDO_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(RP1BUS_FDO_CONTEXT, Rp1BusGetFdoContext)

typedef struct _RP1BUS_PDO_CONTEXT {
    ULONG            Index;
    ULONG            Offset;
    ULONG            Size;
    ULONG            Irq;
    PHYSICAL_ADDRESS ChildPhys;   // BarPhys + Offset
} RP1BUS_PDO_CONTEXT, *PRP1BUS_PDO_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(RP1BUS_PDO_CONTEXT, Rp1BusGetPdoContext)

// Child identification description for the WDF child list.
typedef struct _RP1_CHILD_ID {
    WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER Header;
    ULONG Index;
} RP1_CHILD_ID, *PRP1_CHILD_ID;

EVT_WDF_DRIVER_DEVICE_ADD            Rp1BusEvtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE      Rp1BusEvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE      Rp1BusEvtDeviceReleaseHardware;
EVT_WDF_CHILD_LIST_CREATE_DEVICE     Rp1BusEvtChildListCreateDevice;
