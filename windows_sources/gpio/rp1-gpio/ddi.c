/*++
Module Name: ddi.c
Abstract:    GpioClx client callbacks for the RP1 GPIO controller. Pin read/write
             /connect now drive the real RP1 RIO + CTRL registers via the HAL.
             Interrupt callbacks remain minimal (real demux ties into rp1bus #4).
--*/
#include "common.h"

NTSTATUS GPIO_EXPORT
Rp1GpioPrepareController(_In_ WDFDEVICE Device, _In_ PVOID Context,
    _In_ WDFCMRESLIST ResourcesRaw, _In_ WDFCMRESLIST ResourcesTranslated)
{
    PRP1GPIO_CONTEXT ctx = (PRP1GPIO_CONTEXT)Context;
    ULONG            count = WdfCmResourceListGetCount(ResourcesTranslated);
    ULONG            i;
    ULONG            memIdx = 0;

    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(ResourcesRaw);

    // RP1 GPIO exposes 3 MMIO regions: [0] gpio io, [1] rio, [2] pads.
    for (i = 0; i < count; i++) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR d = WdfCmResourceListGetDescriptor(ResourcesTranslated, i);
        PUCHAR va;
        if (d == NULL || d->Type != CmResourceTypeMemory) {
            continue;
        }
        va = (PUCHAR)MmMapIoSpaceEx(d->u.Memory.Start, d->u.Memory.Length, PAGE_READWRITE | PAGE_NOCACHE);
        switch (memIdx) {
        case 0: ctx->Hw.GpioBase = va; ctx->Hw.GpioLen = d->u.Memory.Length; break;
        case 1: ctx->Hw.RioBase  = va; ctx->Hw.RioLen  = d->u.Memory.Length; break;
        case 2: ctx->Hw.PadsBase = va; ctx->Hw.PadsLen = d->u.Memory.Length; break;
        default: if (va) MmUnmapIoSpace(va, d->u.Memory.Length); break;
        }
        memIdx++;
    }
    // No CM memory resources => we were enumerated as an rp1bus child (RP1\GPIO0)
    // rather than described in ACPI. Query the parent bus interface for our
    // already-mapped BAR1 window and slice the three GPIO sub-blocks out of it.
    if (ctx->Hw.GpioBase == NULL) {
        NTSTATUS qs = WdfFdoQueryForInterface(Device, &GUID_RP1BUS_INTERFACE_STANDARD,
                          (PINTERFACE)&ctx->BusIf, sizeof(ctx->BusIf), 1, NULL);
        if (NT_SUCCESS(qs)) {
            PUCHAR win = (PUCHAR)ctx->BusIf.GetRegisterBase(ctx->BusIf.InterfaceHeader.Context);
            ULONG  len = ctx->BusIf.GetRegisterSize(ctx->BusIf.InterfaceHeader.Context);
            if (win != NULL && len >= (RP1_GPIO_PADS_SUBOFF + RP1_GPIO_SUBLEN)) {
                ctx->Hw.GpioBase = win + RP1_GPIO_IO_SUBOFF;   ctx->Hw.GpioLen = RP1_GPIO_SUBLEN;
                ctx->Hw.RioBase  = win + RP1_GPIO_RIO_SUBOFF;  ctx->Hw.RioLen  = RP1_GPIO_SUBLEN;
                ctx->Hw.PadsBase = win + RP1_GPIO_PADS_SUBOFF; ctx->Hw.PadsLen = RP1_GPIO_SUBLEN;
                ctx->FromBusIf = TRUE;   // keep the interface ref until ReleaseController
            } else {
                // window too small / unmapped: drop the ref, fail prepare below.
                ctx->BusIf.InterfaceHeader.InterfaceDereference(ctx->BusIf.InterfaceHeader.Context);
                RtlZeroMemory(&ctx->BusIf, sizeof(ctx->BusIf));
            }
        }
    }

    if (ctx->Hw.GpioBase == NULL) {
        return STATUS_DEVICE_CONFIGURATION_ERROR;   // neither ACPI nor rp1bus gave us a window
    }
    // Degrade gracefully if the platform provided a single combined window.
    if (ctx->Hw.RioBase == NULL)  ctx->Hw.RioBase  = ctx->Hw.GpioBase;
    if (ctx->Hw.PadsBase == NULL) ctx->Hw.PadsBase = ctx->Hw.GpioBase;
    ctx->PinCount = RP1_GPIO_PIN_COUNT;
    return STATUS_SUCCESS;
}

NTSTATUS GPIO_EXPORT
Rp1GpioReleaseController(_In_ WDFDEVICE Device, _In_ PVOID Context)
{
    PRP1GPIO_CONTEXT ctx = (PRP1GPIO_CONTEXT)Context;
    UNREFERENCED_PARAMETER(Device);

    if (ctx->FromBusIf) {
        // The window is owned (mapped) by rp1bus; just release our interface ref.
        ctx->BusIf.InterfaceHeader.InterfaceDereference(ctx->BusIf.InterfaceHeader.Context);
        RtlZeroMemory(&ctx->BusIf, sizeof(ctx->BusIf));
        ctx->FromBusIf = FALSE;
    } else {
        if (ctx->Hw.PadsBase != NULL && ctx->Hw.PadsBase != ctx->Hw.GpioBase) {
            MmUnmapIoSpace(ctx->Hw.PadsBase, ctx->Hw.PadsLen);
        }
        if (ctx->Hw.RioBase != NULL && ctx->Hw.RioBase != ctx->Hw.GpioBase) {
            MmUnmapIoSpace(ctx->Hw.RioBase, ctx->Hw.RioLen);
        }
        if (ctx->Hw.GpioBase != NULL) {
            MmUnmapIoSpace(ctx->Hw.GpioBase, ctx->Hw.GpioLen);
        }
    }
    ctx->Hw.GpioBase = ctx->Hw.RioBase = ctx->Hw.PadsBase = NULL;
    return STATUS_SUCCESS;
}

NTSTATUS GPIO_EXPORT
Rp1GpioStartController(_In_ PVOID Context, _In_ BOOLEAN RestoreContext,
    _In_ WDF_POWER_DEVICE_STATE PreviousPowerState)
{
    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(RestoreContext);
    UNREFERENCED_PARAMETER(PreviousPowerState);
    return STATUS_SUCCESS;
}

NTSTATUS GPIO_EXPORT
Rp1GpioStopController(_In_ PVOID Context, _In_ BOOLEAN SaveContext,
    _In_ WDF_POWER_DEVICE_STATE TargetState)
{
    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(SaveContext);
    UNREFERENCED_PARAMETER(TargetState);
    return STATUS_SUCCESS;
}

NTSTATUS GPIO_EXPORT
Rp1GpioQueryControllerBasicInformation(_In_ PVOID Context,
    _Out_ PCLIENT_CONTROLLER_BASIC_INFORMATION Information)
{
    UNREFERENCED_PARAMETER(Context);
    Information->Version             = GPIO_CONTROLLER_BASIC_INFORMATION_VERSION;
    Information->Size                = sizeof(CLIENT_CONTROLLER_BASIC_INFORMATION);
    Information->TotalPins           = RP1_GPIO_PIN_COUNT;
    Information->NumberOfPinsPerBank = RP1_GPIO_PIN_COUNT;
    Information->DeviceIdleTimeout   = 0;
    RtlZeroMemory(&Information->Flags, sizeof(Information->Flags));
    return STATUS_SUCCESS;
}

NTSTATUS GPIO_EXPORT
Rp1GpioQuerySetControllerInformation(_In_ PVOID Context,
    _In_ PCLIENT_CONTROLLER_QUERY_SET_INFORMATION_INPUT InputBuffer,
    _Out_opt_ PCLIENT_CONTROLLER_QUERY_SET_INFORMATION_OUTPUT OutputBuffer)
{
    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(OutputBuffer);
    return STATUS_NOT_SUPPORTED;
}

/* ---- interrupt callbacks: wired to the RP1 GPIO interrupt registers ---- */
NTSTATUS GPIO_EXPORT
Rp1GpioEnableInterrupt(_In_ PVOID Context, _In_ PGPIO_ENABLE_INTERRUPT_PARAMETERS EnableParameters)
{
    PRP1GPIO_CONTEXT ctx = (PRP1GPIO_CONTEXT)Context;
    BOOLEAN edge    = (BOOLEAN)(EnableParameters->InterruptMode == Latched);
    BOOLEAN high    = (BOOLEAN)(!edge && EnableParameters->Polarity == InterruptActiveHigh);
    BOOLEAN low     = (BOOLEAN)(!edge && EnableParameters->Polarity == InterruptActiveLow);
    BOOLEAN rising  = (BOOLEAN)( edge && EnableParameters->Polarity == InterruptActiveHigh);
    BOOLEAN falling = (BOOLEAN)( edge && EnableParameters->Polarity == InterruptActiveLow);
    Rp1GpioEnableIrq(&ctx->Hw, EnableParameters->PinNumber, rising, falling, high, low);
    return STATUS_SUCCESS;
}

NTSTATUS GPIO_EXPORT
Rp1GpioDisableInterrupt(_In_ PVOID Context, _In_ PGPIO_DISABLE_INTERRUPT_PARAMETERS DisableParameters)
{
    PRP1GPIO_CONTEXT ctx = (PRP1GPIO_CONTEXT)Context;
    Rp1GpioMaskIrq(&ctx->Hw, DisableParameters->PinNumber);
    return STATUS_SUCCESS;
}

NTSTATUS GPIO_EXPORT
Rp1GpioUnmaskInterrupt(_In_ PVOID Context, _In_ PGPIO_ENABLE_INTERRUPT_PARAMETERS InterruptParameters)
{
    PRP1GPIO_CONTEXT ctx = (PRP1GPIO_CONTEXT)Context;
    Rp1GpioUnmaskIrq(&ctx->Hw, InterruptParameters->PinNumber);
    return STATUS_SUCCESS;
}

NTSTATUS GPIO_EXPORT
Rp1GpioMaskInterrupts(_In_ PVOID Context, _In_ PGPIO_MASK_INTERRUPT_PARAMETERS MaskParameters)
{
    PRP1GPIO_CONTEXT ctx = (PRP1GPIO_CONTEXT)Context;
    ULONG pin;
    for (pin = 0; pin < RP1_GPIO_PIN_COUNT; pin++) {
        if (MaskParameters->PinMask & (1ull << pin)) {
            Rp1GpioMaskIrq(&ctx->Hw, pin);
        }
    }
    MaskParameters->FailedMask = 0;
    return STATUS_SUCCESS;
}

NTSTATUS GPIO_EXPORT
Rp1GpioQueryActiveInterrupts(_In_ PVOID Context, _In_ PGPIO_QUERY_ACTIVE_INTERRUPTS_PARAMETERS QueryActiveParameters)
{
    PRP1GPIO_CONTEXT ctx = (PRP1GPIO_CONTEXT)Context;
    ULONG64 active = 0;
    /* aggregate the 3 hardware banks into the single 54-pin GpioClx bank */
    active |= (ULONG64)Rp1GpioQueryActiveIrq(&ctx->Hw, 0);
    active |= (ULONG64)Rp1GpioQueryActiveIrq(&ctx->Hw, 1) << 28;
    active |= (ULONG64)Rp1GpioQueryActiveIrq(&ctx->Hw, 2) << 34;
    QueryActiveParameters->ActiveMask = active & QueryActiveParameters->EnabledMask;
    return STATUS_SUCCESS;
}

NTSTATUS GPIO_EXPORT
Rp1GpioClearActiveInterrupts(_In_ PVOID Context, _In_ PGPIO_CLEAR_ACTIVE_INTERRUPTS_PARAMETERS ClearParameters)
{
    PRP1GPIO_CONTEXT ctx = (PRP1GPIO_CONTEXT)Context;
    ULONG pin;
    for (pin = 0; pin < RP1_GPIO_PIN_COUNT; pin++) {
        if (ClearParameters->ClearActiveMask & (1ull << pin)) {
            Rp1GpioAckIrq(&ctx->Hw, pin);
        }
    }
    ClearParameters->FailedClearMask = 0;
    return STATUS_SUCCESS;
}

/* ---- IO callbacks: drive the real RP1 RIO/CTRL registers ---- */
NTSTATUS GPIO_EXPORT
Rp1GpioConnectIoPins(_In_ PVOID Context, _In_ PGPIO_CONNECT_IO_PINS_PARAMETERS ConnectParameters)
{
    PRP1GPIO_CONTEXT ctx = (PRP1GPIO_CONTEXT)Context;
    BOOLEAN output = (ConnectParameters->ConnectMode == ConnectModeOutput);
    ULONG pud;
    ULONG i;

    /* Map the GpioClx pull request to the RP1 PADS bias (was previously dropped,
     * leaving inputs floating). GpioPinPull: Default=0, Up=1, Down=2, None=3. */
    switch (ConnectParameters->PullConfiguration) {
    case GPIO_PIN_PULL_CONFIGURATION_PULLUP:   pud = RP1_PUD_UP;   break;
    case GPIO_PIN_PULL_CONFIGURATION_PULLDOWN: pud = RP1_PUD_DOWN; break;
    default:                                   pud = RP1_PUD_OFF;  break;   /* None / Default */
    }

    for (i = 0; i < ConnectParameters->PinCount; i++) {
        ULONG pin = ConnectParameters->PinNumberTable[i];
        Rp1GpioSelectGpioFunction(&ctx->Hw, pin);
        Rp1GpioSetPull(&ctx->Hw, pin, pud);
        Rp1GpioSetDirection(&ctx->Hw, pin, output);
    }
    return STATUS_SUCCESS;
}

NTSTATUS GPIO_EXPORT
Rp1GpioDisconnectIoPins(_In_ PVOID Context, _In_ PGPIO_DISCONNECT_IO_PINS_PARAMETERS DisconnectParameters)
{
    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(DisconnectParameters);
    return STATUS_SUCCESS;
}

NTSTATUS GPIO_EXPORT
Rp1GpioReadGpioPins(_In_ PVOID Context, _In_ PGPIO_READ_PINS_PARAMETERS ReadParameters)
{
    PRP1GPIO_CONTEXT ctx = (PRP1GPIO_CONTEXT)Context;
    UINT64 *out = (UINT64 *)ReadParameters->Buffer;
    ULONG i;
    if (out != NULL) {
        *out = 0;
    }
    for (i = 0; i < ReadParameters->PinCount; i++) {
        if (Rp1GpioReadPin(&ctx->Hw, ReadParameters->PinNumberTable[i]) && out != NULL) {
            *out |= (1ull << i);
        }
    }
    return STATUS_SUCCESS;
}

NTSTATUS GPIO_EXPORT
Rp1GpioWriteGpioPins(_In_ PVOID Context, _In_ PGPIO_WRITE_PINS_PARAMETERS WriteParameters)
{
    PRP1GPIO_CONTEXT ctx = (PRP1GPIO_CONTEXT)Context;
    UINT64 vals = (WriteParameters->Buffer != NULL) ? *(UINT64 *)WriteParameters->Buffer : 0;
    ULONG i;
    for (i = 0; i < WriteParameters->PinCount; i++) {
        Rp1GpioWritePin(&ctx->Hw, WriteParameters->PinNumberTable[i],
                        (vals & (1ull << i)) ? TRUE : FALSE);
    }
    return STATUS_SUCCESS;
}
