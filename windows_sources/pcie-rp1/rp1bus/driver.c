/*++
Module Name: driver.c
Abstract:    RP1 PCIe bus driver - DriverEntry, FDO add, BAR1 mapping, and static
             enumeration of RP1 internal peripherals as child PDOs.

    Scope of this skeleton: bind PCI, map BAR1, and create the child PDOs with
    correct hardware IDs + per-child BAR1 slice info. Reporting the sliced MMIO
    and the GpioInt-backed interrupt as PDO raw resources, plus the GpioClx
    interrupt-controller role (MSI-X demux), are the next refinements.
--*/
#define INITGUID            // instantiate GUID_RP1BUS_INTERFACE_STANDARD here
#include "common.h"

DRIVER_INITIALIZE DriverEntry;

// RP1 internal peripheral map (offsets within BAR1; full set per RP1 datasheet
// + Pi5 device-tree rp1.dtsi). Offset = (DT reg low 32 bits) - 0x40000000;
// Irq = RP1 internal interrupt-controller index (RP1_INT_*).
static const RP1_PERIPH g_Rp1Periph[] = {
    { L"RP1\\UART0", L"0", 0x030000, 0x200,   25 },
    { L"RP1\\UART1", L"1", 0x034000, 0x200,   42 },
    { L"RP1\\UART2", L"2", 0x038000, 0x200,   43 },
    { L"RP1\\I2C0",  L"0", 0x070000, 0x1000,   7 },
    { L"RP1\\I2C1",  L"1", 0x074000, 0x1000,   8 },
    { L"RP1\\SPI0",  L"0", 0x050000, 0x400,   19 },
    { L"RP1\\SPI1",  L"1", 0x054000, 0x400,   20 },
    { L"RP1\\I2S0",  L"0", 0x0A0000, 0x1000,  14 },
    { L"RP1\\GPIO0", L"0", 0x0D0000, 0xC000,   0 },  // RIO+pads+gpio bank window
    { L"RP1\\ETH0",  L"0", 0x100000, 0x4000,   6 },
    { L"RP1\\USB0",  L"0", 0x200000, 0x100000,30 },  // xHCI #0
    { L"RP1\\USB1",  L"1", 0x300000, 0x100000,31 },  // xHCI #1
    { L"RP1\\PWM0",  L"0", 0x098000, 0x100,    5 },
    { L"RP1\\ADC0",  L"0", 0x0C8000, 0x100,   38 },
    { L"RP1\\DMA0",  L"0", 0x188000, 0x1000,  40 },
};

//
// ---- Bus interface exported to child PDOs (GUID_RP1BUS_INTERFACE_STANDARD) ----
// The interface Context is the child WDFDEVICE; callbacks read its PDO context
// and the live parent BarBase, so a class driver gets its mapped window + IRQ.
//
static VOID Rp1BusIfReference(_In_ PVOID Context)   { WdfObjectReference((WDFOBJECT)Context); }
static VOID Rp1BusIfDereference(_In_ PVOID Context) { WdfObjectDereference((WDFOBJECT)Context); }

static PVOID
Rp1BusIfGetRegisterBase(_In_ PVOID Context)
{
    PRP1BUS_PDO_CONTEXT pdo = Rp1BusGetPdoContext((WDFDEVICE)Context);
    if (pdo->Fdo == NULL || pdo->Fdo->BarBase == NULL) {
        return NULL;
    }
    return (PVOID)((PUCHAR)pdo->Fdo->BarBase + pdo->Offset);
}

static ULONG
Rp1BusIfGetRegisterSize(_In_ PVOID Context)
{
    return Rp1BusGetPdoContext((WDFDEVICE)Context)->Size;
}

static ULONG
Rp1BusIfGetInterruptIndex(_In_ PVOID Context)
{
    return Rp1BusGetPdoContext((WDFDEVICE)Context)->Irq;
}

static PHYSICAL_ADDRESS
Rp1BusIfGetPhysicalBase(_In_ PVOID Context)
{
    return Rp1BusGetPdoContext((WDFDEVICE)Context)->ChildPhys;
}

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, Rp1BusEvtDeviceAdd)
#endif

NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, Rp1BusEvtDeviceAdd);
    return WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES,
                           &config, WDF_NO_HANDLE);
}

NTSTATUS
Rp1BusEvtDeviceAdd(_In_ WDFDRIVER Driver, _Inout_ PWDFDEVICE_INIT DeviceInit)
{
    WDF_PNPPOWER_EVENT_CALLBACKS pnp;
    WDF_CHILD_LIST_CONFIG        childCfg;
    WDF_OBJECT_ATTRIBUTES        attribs;
    WDFDEVICE                    device;
    NTSTATUS                     status;

    UNREFERENCED_PARAMETER(Driver);
    PAGED_CODE();

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
    pnp.EvtDevicePrepareHardware = Rp1BusEvtDevicePrepareHardware;
    pnp.EvtDeviceReleaseHardware = Rp1BusEvtDeviceReleaseHardware;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnp);

    // Static child list: RP1 enumerates a fixed set of internal peripherals.
    WDF_CHILD_LIST_CONFIG_INIT(&childCfg, sizeof(RP1_CHILD_ID),
                               Rp1BusEvtChildListCreateDevice);
    WdfFdoInitSetDefaultChildListConfig(DeviceInit, &childCfg,
                                        WDF_NO_OBJECT_ATTRIBUTES);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attribs, RP1BUS_FDO_CONTEXT);
    status = WdfDeviceCreate(&DeviceInit, &attribs, &device);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    return STATUS_SUCCESS;
}

NTSTATUS
Rp1BusEvtDevicePrepareHardware(_In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesRaw, _In_ WDFCMRESLIST ResourcesTranslated)
{
    PRP1BUS_FDO_CONTEXT ctx = Rp1BusGetFdoContext(Device);
    ULONG               count = WdfCmResourceListGetCount(ResourcesTranslated);
    PCM_PARTIAL_RESOURCE_DESCRIPTOR bar = NULL;
    WDFCHILDLIST        children;
    ULONG               i;

    UNREFERENCED_PARAMETER(ResourcesRaw);

    // RP1 uses BAR1 (the large >64KB window). Pick the largest memory resource.
    for (i = 0; i < count; i++) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR d = WdfCmResourceListGetDescriptor(ResourcesTranslated, i);
        if (d != NULL && d->Type == CmResourceTypeMemory) {
            if (bar == NULL || d->u.Memory.Length > bar->u.Memory.Length) {
                bar = d;
            }
        }
    }
    if (bar == NULL) {
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    ctx->BarPhys   = bar->u.Memory.Start;
    ctx->BarLength = bar->u.Memory.Length;
    ctx->BarBase   = MmMapIoSpaceEx(bar->u.Memory.Start, bar->u.Memory.Length,
                                    PAGE_READWRITE | PAGE_NOCACHE);
    if (ctx->BarBase == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // RP1 firmware must be running for BAR1 to be the full size.
    if (ctx->BarLength <= 0x10000) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_WARNING_LEVEL,
                   "rp1bus: BAR1 too small (0x%Ix) - RP1 firmware not up?\n", ctx->BarLength);
    }

    // Enumerate the internal peripherals as child PDOs.
    children = WdfFdoGetDefaultChildList(Device);
    WdfChildListBeginScan(children);
    for (i = 0; i < RTL_NUMBER_OF(g_Rp1Periph); i++) {
        RP1_CHILD_ID id;
        WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER_INIT(&id.Header, sizeof(id));
        id.Index = i;
        (VOID)WdfChildListAddOrUpdateChildDescriptionAsPresent(children, &id.Header, NULL);
    }
    WdfChildListEndScan(children);

    return STATUS_SUCCESS;
}

NTSTATUS
Rp1BusEvtDeviceReleaseHardware(_In_ WDFDEVICE Device, _In_ WDFCMRESLIST ResourcesTranslated)
{
    PRP1BUS_FDO_CONTEXT ctx = Rp1BusGetFdoContext(Device);
    UNREFERENCED_PARAMETER(ResourcesTranslated);
    if (ctx->BarBase != NULL) {
        MmUnmapIoSpace(ctx->BarBase, ctx->BarLength);
        ctx->BarBase = NULL;
    }
    return STATUS_SUCCESS;
}

NTSTATUS
Rp1BusEvtChildListCreateDevice(_In_ WDFCHILDLIST ChildList,
    _In_ PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER IdentificationDescription,
    _In_ PWDFDEVICE_INIT ChildInit)
{
    PRP1_CHILD_ID         id = CONTAINING_RECORD(IdentificationDescription, RP1_CHILD_ID, Header);
    const RP1_PERIPH *    p  = &g_Rp1Periph[id->Index];
    WDFDEVICE             parent = WdfChildListGetDevice(ChildList);
    PRP1BUS_FDO_CONTEXT   fdo = Rp1BusGetFdoContext(parent);
    WDF_OBJECT_ATTRIBUTES attribs;
    WDFDEVICE             child;
    PRP1BUS_PDO_CONTEXT   pdoCtx;
    UNICODE_STRING        str;
    NTSTATUS              status;

    // Hardware ID (e.g. "RP1\UART0") so the matching class driver loads.
    RtlInitUnicodeString(&str, p->HardwareId);
    status = WdfPdoInitAssignDeviceID(ChildInit, &str);
    if (NT_SUCCESS(status)) status = WdfPdoInitAddHardwareID(ChildInit, &str);

    if (NT_SUCCESS(status)) {
        RtlInitUnicodeString(&str, p->InstanceId);
        status = WdfPdoInitAssignInstanceID(ChildInit, &str);
    }
    if (!NT_SUCCESS(status)) {
        return status;
    }

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attribs, RP1BUS_PDO_CONTEXT);
    status = WdfDeviceCreate(&ChildInit, &attribs, &child);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Record this child's slice of BAR1 + a link back to the parent FDO ctx so
    // the bus interface can hand out (live BarBase + Offset) and the IRQ index.
    pdoCtx = Rp1BusGetPdoContext(child);
    pdoCtx->Index = id->Index;
    pdoCtx->Offset = p->Offset;
    pdoCtx->Size = p->Size;
    pdoCtx->Irq = p->Irq;
    pdoCtx->ChildPhys.QuadPart = fdo->BarPhys.QuadPart + p->Offset;
    pdoCtx->Fdo = fdo;

    // Export the RP1 bus interface on this child PDO. The class driver queries
    // it (WdfFdoQueryForInterface) to get its mapped window + interrupt index.
    {
        RP1BUS_INTERFACE_STANDARD  ifc;
        WDF_QUERY_INTERFACE_CONFIG ifCfg;

        RtlZeroMemory(&ifc, sizeof(ifc));
        ifc.InterfaceHeader.Size                = sizeof(ifc);
        ifc.InterfaceHeader.Version             = 1;
        ifc.InterfaceHeader.Context             = (PVOID)child;
        ifc.InterfaceHeader.InterfaceReference  = Rp1BusIfReference;
        ifc.InterfaceHeader.InterfaceDereference= Rp1BusIfDereference;
        ifc.GetRegisterBase   = Rp1BusIfGetRegisterBase;
        ifc.GetRegisterSize   = Rp1BusIfGetRegisterSize;
        ifc.GetInterruptIndex = Rp1BusIfGetInterruptIndex;
        ifc.GetPhysicalBase   = Rp1BusIfGetPhysicalBase;

        WDF_QUERY_INTERFACE_CONFIG_INIT(&ifCfg, (PINTERFACE)&ifc,
                                        &GUID_RP1BUS_INTERFACE_STANDARD, NULL);
        status = WdfDeviceAddQueryInterface(child, &ifCfg);
        if (!NT_SUCCESS(status)) {
            return status;
        }
    }

    return STATUS_SUCCESS;
}
