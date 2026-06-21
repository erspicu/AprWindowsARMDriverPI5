/*++
Module Name: ddi.c
Abstract:    GpioClx client callbacks for BCM2712 GPIO, wired to brcm_gpio_hw.c.
             Reported as 2 banks x 32 pins, so GpioClx BankId == our bank and the
             pin numbers are bank-relative (0..31) - a direct HAL mapping.
--*/
#include "common.h"

NTSTATUS GPIO_EXPORT
BcmGpioPrepareController(_In_ WDFDEVICE Device, _In_ PVOID Context,
    _In_ WDFCMRESLIST ResourcesRaw, _In_ WDFCMRESLIST ResourcesTranslated)
{
    PBCMGPIO_CONTEXT ctx = (PBCMGPIO_CONTEXT)Context;
    ULONG i, count = WdfCmResourceListGetCount(ResourcesTranslated);
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(ResourcesRaw);
    for (i = 0; i < count; i++) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR d = WdfCmResourceListGetDescriptor(ResourcesTranslated, i);
        if (d != NULL && d->Type == CmResourceTypeMemory) {
            ctx->Length = d->u.Memory.Length;
            ctx->Base   = MmMapIoSpaceEx(d->u.Memory.Start, d->u.Memory.Length, PAGE_READWRITE | PAGE_NOCACHE);
            break;
        }
    }
    return STATUS_SUCCESS;
}

NTSTATUS GPIO_EXPORT
BcmGpioReleaseController(_In_ WDFDEVICE Device, _In_ PVOID Context)
{
    PBCMGPIO_CONTEXT ctx = (PBCMGPIO_CONTEXT)Context;
    UNREFERENCED_PARAMETER(Device);
    if (ctx->Base != NULL) { MmUnmapIoSpace(ctx->Base, ctx->Length); ctx->Base = NULL; }
    return STATUS_SUCCESS;
}

NTSTATUS GPIO_EXPORT
BcmGpioStartController(_In_ PVOID Context, _In_ BOOLEAN RestoreContext, _In_ WDF_POWER_DEVICE_STATE PreviousPowerState)
{ UNREFERENCED_PARAMETER(Context); UNREFERENCED_PARAMETER(RestoreContext); UNREFERENCED_PARAMETER(PreviousPowerState); return STATUS_SUCCESS; }

NTSTATUS GPIO_EXPORT
BcmGpioStopController(_In_ PVOID Context, _In_ BOOLEAN SaveContext, _In_ WDF_POWER_DEVICE_STATE TargetState)
{ UNREFERENCED_PARAMETER(Context); UNREFERENCED_PARAMETER(SaveContext); UNREFERENCED_PARAMETER(TargetState); return STATUS_SUCCESS; }

NTSTATUS GPIO_EXPORT
BcmGpioQueryControllerBasicInformation(_In_ PVOID Context, _Out_ PCLIENT_CONTROLLER_BASIC_INFORMATION Information)
{
    UNREFERENCED_PARAMETER(Context);
    Information->Version             = GPIO_CONTROLLER_BASIC_INFORMATION_VERSION;
    Information->Size                = sizeof(CLIENT_CONTROLLER_BASIC_INFORMATION);
    Information->TotalPins           = BCM_GPIO_TOTAL_PINS;
    Information->NumberOfPinsPerBank = BCM_GPIO_PINS_PER_BANK;
    Information->DeviceIdleTimeout   = 0;
    RtlZeroMemory(&Information->Flags, sizeof(Information->Flags));
    return STATUS_SUCCESS;
}

NTSTATUS GPIO_EXPORT
BcmGpioQuerySetControllerInformation(_In_ PVOID Context,
    _In_ PCLIENT_CONTROLLER_QUERY_SET_INFORMATION_INPUT InputBuffer,
    _Out_opt_ PCLIENT_CONTROLLER_QUERY_SET_INFORMATION_OUTPUT OutputBuffer)
{ UNREFERENCED_PARAMETER(Context); UNREFERENCED_PARAMETER(InputBuffer); UNREFERENCED_PARAMETER(OutputBuffer); return STATUS_NOT_SUPPORTED; }

NTSTATUS GPIO_EXPORT
BcmGpioEnableInterrupt(_In_ PVOID Context, _In_ PGPIO_ENABLE_INTERRUPT_PARAMETERS p)
{
    PBCMGPIO_CONTEXT ctx = (PBCMGPIO_CONTEXT)Context;
    int edge = (p->InterruptMode == Latched);
    int high = (p->Polarity == InterruptActiveHigh);
    BcmGpioEnableIrq(ctx->Base, p->BankId, p->PinNumber, edge, 0, high);
    return STATUS_SUCCESS;
}

NTSTATUS GPIO_EXPORT
BcmGpioDisableInterrupt(_In_ PVOID Context, _In_ PGPIO_DISABLE_INTERRUPT_PARAMETERS p)
{ PBCMGPIO_CONTEXT ctx = (PBCMGPIO_CONTEXT)Context; BcmGpioMaskIrq(ctx->Base, p->BankId, p->PinNumber); return STATUS_SUCCESS; }

NTSTATUS GPIO_EXPORT
BcmGpioUnmaskInterrupt(_In_ PVOID Context, _In_ PGPIO_ENABLE_INTERRUPT_PARAMETERS p)
{ PBCMGPIO_CONTEXT ctx = (PBCMGPIO_CONTEXT)Context; BcmGpioUnmaskIrq(ctx->Base, p->BankId, p->PinNumber); return STATUS_SUCCESS; }

NTSTATUS GPIO_EXPORT
BcmGpioMaskInterrupts(_In_ PVOID Context, _In_ PGPIO_MASK_INTERRUPT_PARAMETERS p)
{
    PBCMGPIO_CONTEXT ctx = (PBCMGPIO_CONTEXT)Context;
    unsigned pin;
    for (pin = 0; pin < BCM_GPIO_PINS_PER_BANK; pin++)
        if (p->PinMask & (1ull << pin)) BcmGpioMaskIrq(ctx->Base, p->BankId, pin);
    p->FailedMask = 0;
    return STATUS_SUCCESS;
}

NTSTATUS GPIO_EXPORT
BcmGpioQueryActiveInterrupts(_In_ PVOID Context, _In_ PGPIO_QUERY_ACTIVE_INTERRUPTS_PARAMETERS p)
{
    PBCMGPIO_CONTEXT ctx = (PBCMGPIO_CONTEXT)Context;
    p->ActiveMask = (ULONG64)BcmGpioQueryActive(ctx->Base, p->BankId) & p->EnabledMask;
    return STATUS_SUCCESS;
}

NTSTATUS GPIO_EXPORT
BcmGpioClearActiveInterrupts(_In_ PVOID Context, _In_ PGPIO_CLEAR_ACTIVE_INTERRUPTS_PARAMETERS p)
{
    PBCMGPIO_CONTEXT ctx = (PBCMGPIO_CONTEXT)Context;
    unsigned pin;
    for (pin = 0; pin < BCM_GPIO_PINS_PER_BANK; pin++)
        if (p->ClearActiveMask & (1ull << pin)) BcmGpioAckIrq(ctx->Base, p->BankId, pin);
    p->FailedClearMask = 0;
    return STATUS_SUCCESS;
}

NTSTATUS GPIO_EXPORT
BcmGpioConnectIoPins(_In_ PVOID Context, _In_ PGPIO_CONNECT_IO_PINS_PARAMETERS p)
{
    PBCMGPIO_CONTEXT ctx = (PBCMGPIO_CONTEXT)Context;
    int output = (p->ConnectMode == ConnectModeOutput);
    ULONG i;
    for (i = 0; i < p->PinCount; i++)
        BcmGpioSetDirection(ctx->Base, p->BankId, p->PinNumberTable[i], output);
    return STATUS_SUCCESS;
}

NTSTATUS GPIO_EXPORT
BcmGpioDisconnectIoPins(_In_ PVOID Context, _In_ PGPIO_DISCONNECT_IO_PINS_PARAMETERS p)
{ UNREFERENCED_PARAMETER(Context); UNREFERENCED_PARAMETER(p); return STATUS_SUCCESS; }

NTSTATUS GPIO_EXPORT
BcmGpioReadGpioPins(_In_ PVOID Context, _In_ PGPIO_READ_PINS_PARAMETERS p)
{
    PBCMGPIO_CONTEXT ctx = (PBCMGPIO_CONTEXT)Context;
    UINT64 *out = (UINT64 *)p->Buffer;
    ULONG i;
    if (out != NULL) *out = 0;
    for (i = 0; i < p->PinCount; i++)
        if (BcmGpioReadPin(ctx->Base, p->BankId, p->PinNumberTable[i]) && out != NULL) *out |= (1ull << i);
    return STATUS_SUCCESS;
}

NTSTATUS GPIO_EXPORT
BcmGpioWriteGpioPins(_In_ PVOID Context, _In_ PGPIO_WRITE_PINS_PARAMETERS p)
{
    PBCMGPIO_CONTEXT ctx = (PBCMGPIO_CONTEXT)Context;
    UINT64 vals = (p->Buffer != NULL) ? *(UINT64 *)p->Buffer : 0;
    ULONG i;
    for (i = 0; i < p->PinCount; i++)
        BcmGpioWritePin(ctx->Base, p->BankId, p->PinNumberTable[i], (vals & (1ull << i)) ? 1 : 0);
    return STATUS_SUCCESS;
}
