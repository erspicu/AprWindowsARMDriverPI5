/*++
Module Name: uart.c
Abstract:    BCM4345C0 BT transport - WDF glue to the UART + reset GPIO (Phase B).
             B1: parse the ACPI _CRS (UARTSerialBusV2 + GpioIo) and open both as
                 WDFIOTARGETs (RESOURCE_HUB path).
             B2: drive the bring-up state machine (init_sm.c) over the real UART -
                 send via the injected Tx, read synchronously and feed the H4 RX
                 reassembler, which advances the state machine on each Command
                 Complete; then switch the host UART to the operational baud.
             B3 (start of): synchronous read helper reused by an async RX pump.

   Compiles under the WDK /kernel ARM64 toolchain. Functional bring-up needs the
   Pi5 running Win11 ARM64 (real UART + sdbus-less; this is the BT half).
   Pi5 facts wired in: UART = BCM2712 PL011 (brcm,bcm7271-uart) @ serial@7d50c000;
   reset = shutdown-gpios (GPIO29); operational baud 3,000,000.
--*/
#include "common.h"
#include <ntstrsafe.h>   /* reshub.h's RESOURCE_HUB_*_PRINTF path helpers need this */
#include <ntddser.h>
#include <gpio.h>
#define RESHUB_USE_HELPER_ROUTINES   /* gate for RESOURCE_HUB_CREATE_PATH_FROM_ID */
#include <reshub.h>

/* ---- B1: resources ---- */
NTSTATUS
BtBcmParseResources(_Inout_ PBTBCM_CONTEXT Ctx, _In_ WDFCMRESLIST ResTranslated)
{
    ULONG i, count = WdfCmResourceListGetCount(ResTranslated);
    PCM_PARTIAL_RESOURCE_DESCRIPTOR d;
    BOOLEAN haveUart = FALSE;

    Ctx->HasGpio = FALSE;
    for (i = 0; i < count; i++) {
        d = WdfCmResourceListGetDescriptor(ResTranslated, i);
        if (d == NULL || d->Type != CmResourceTypeConnection) {
            continue;
        }
        if (d->u.Connection.Class == CM_RESOURCE_CONNECTION_CLASS_SERIAL &&
            d->u.Connection.Type  == CM_RESOURCE_CONNECTION_TYPE_SERIAL_UART) {
            Ctx->UartConnId.LowPart  = d->u.Connection.IdLowPart;
            Ctx->UartConnId.HighPart = d->u.Connection.IdHighPart;
            haveUart = TRUE;
        } else if (d->u.Connection.Class == CM_RESOURCE_CONNECTION_CLASS_GPIO) {
            Ctx->GpioConnId.LowPart  = d->u.Connection.IdLowPart;
            Ctx->GpioConnId.HighPart = d->u.Connection.IdHighPart;
            Ctx->HasGpio = TRUE;
        }
    }
    return haveUart ? STATUS_SUCCESS : STATUS_DEVICE_CONFIGURATION_ERROR;
}

static NTSTATUS
BtBcmOpenById(_In_ PBTBCM_CONTEXT Ctx, _In_ LARGE_INTEGER ConnId, _Out_ WDFIOTARGET *Target)
{
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attr;
    WDF_IO_TARGET_OPEN_PARAMS openParams;
    DECLARE_UNICODE_STRING_SIZE(path, RESOURCE_HUB_PATH_SIZE);

    *Target = NULL;
    RESOURCE_HUB_CREATE_PATH_FROM_ID(&path, ConnId.LowPart, ConnId.HighPart);

    WDF_OBJECT_ATTRIBUTES_INIT(&attr);
    attr.ParentObject = Ctx->Device;
    status = WdfIoTargetCreate(Ctx->Device, &attr, Target);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&openParams, &path,
                                                GENERIC_READ | GENERIC_WRITE);
    status = WdfIoTargetOpen(*Target, &openParams);
    if (!NT_SUCCESS(status)) {
        WdfObjectDelete(*Target);
        *Target = NULL;
    }
    return status;
}

NTSTATUS
BtBcmOpenTargets(_Inout_ PBTBCM_CONTEXT Ctx)
{
    NTSTATUS status = BtBcmOpenById(Ctx, Ctx->UartConnId, &Ctx->UartTarget);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    if (Ctx->HasGpio) {
        /* a GPIO open failure is non-fatal for bring-up debugging */
        (void)BtBcmOpenById(Ctx, Ctx->GpioConnId, &Ctx->GpioTarget);
    }
    return STATUS_SUCCESS;
}

/* ---- GPIO BT_REG_ON ---- */
NTSTATUS
BtBcmGpioWrite(_In_ PBTBCM_CONTEXT Ctx, _In_ BOOLEAN High)
{
    NTSTATUS status;
    ULONG64  value = High ? 1ull : 0ull;   /* one bit per pin in the GpioIo resource */
    WDF_MEMORY_DESCRIPTOR inDesc, outDesc;

    if (Ctx->GpioTarget == NULL) {
        return STATUS_DEVICE_NOT_READY;
    }
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inDesc,  &value, sizeof(value));
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outDesc, &value, sizeof(value));
    status = WdfIoTargetSendIoctlSynchronously(Ctx->GpioTarget, NULL,
                 IOCTL_GPIO_WRITE_PINS, &inDesc, &outDesc, NULL, NULL);
    return status;
}

/* ---- UART control ---- */
NTSTATUS
BtBcmUartSetBaud(_In_ PBTBCM_CONTEXT Ctx, _In_ ULONG Baud)
{
    SERIAL_BAUD_RATE br;
    WDF_MEMORY_DESCRIPTOR inDesc;
    br.BaudRate = Baud;
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inDesc, &br, sizeof(br));
    return WdfIoTargetSendIoctlSynchronously(Ctx->UartTarget, NULL,
               IOCTL_SERIAL_SET_BAUD_RATE, &inDesc, NULL, NULL, NULL);
}

NTSTATUS
BtBcmUartPurge(_In_ PBTBCM_CONTEXT Ctx)
{
    ULONG mask = SERIAL_PURGE_RXABORT | SERIAL_PURGE_RXCLEAR |
                 SERIAL_PURGE_TXABORT | SERIAL_PURGE_TXCLEAR;
    WDF_MEMORY_DESCRIPTOR inDesc;
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inDesc, &mask, sizeof(mask));
    return WdfIoTargetSendIoctlSynchronously(Ctx->UartTarget, NULL,
               IOCTL_SERIAL_PURGE, &inDesc, NULL, NULL, NULL);
}

NTSTATUS
BtBcmUartWriteSync(_In_ PBTBCM_CONTEXT Ctx, _In_reads_(Len) PUCHAR Buf, _In_ ULONG Len)
{
    WDF_MEMORY_DESCRIPTOR memDesc;
    WDF_REQUEST_SEND_OPTIONS opts;
    WDF_REQUEST_SEND_OPTIONS_INIT(&opts, WDF_REQUEST_SEND_OPTION_TIMEOUT);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&opts, WDF_REL_TIMEOUT_IN_SEC(2));
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memDesc, Buf, Len);
    return WdfIoTargetSendWriteSynchronously(Ctx->UartTarget, NULL, &memDesc,
                                             NULL, &opts, NULL);
}

NTSTATUS
BtBcmUartReadSync(_In_ PBTBCM_CONTEXT Ctx, _Out_writes_(Len) PUCHAR Buf, _In_ ULONG Len,
                  _In_ ULONG TimeoutMs, _Out_ PULONG BytesRead)
{
    WDF_MEMORY_DESCRIPTOR memDesc;
    WDF_REQUEST_SEND_OPTIONS opts;
    ULONG_PTR bytes = 0;
    NTSTATUS status;

    *BytesRead = 0;
    WDF_REQUEST_SEND_OPTIONS_INIT(&opts, WDF_REQUEST_SEND_OPTION_TIMEOUT);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&opts, WDF_REL_TIMEOUT_IN_MS(TimeoutMs));
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memDesc, Buf, Len);
    status = WdfIoTargetSendReadSynchronously(Ctx->UartTarget, NULL, &memDesc,
                                              NULL, &opts, &bytes);
    if (NT_SUCCESS(status)) {
        *BytesRead = (ULONG)bytes;
    }
    return status;
}

/* ---- B2: bring-up driven by init_sm.c over the real UART ---- */
static int
BtBcmUartTx(PVOID Ctx, const UCHAR *Data, ULONG Len)
{
    PBTBCM_CONTEXT c = (PBTBCM_CONTEXT)Ctx;
    NTSTATUS st = BtBcmUartWriteSync(c, (PUCHAR)Data, Len);
    return NT_SUCCESS(st) ? 0 : -1;
}

static VOID
BtBcmInitRxCb(PVOID Context, const UCHAR *Packet, ULONG Length)
{
    PBTBCM_CONTEXT c = (PBTBCM_CONTEXT)Context;
    BtBcmInitOnEvent(&c->Bti, Packet, Length);   /* advances the state machine */
}

NTSTATUS
BtBcmBringUp(_Inout_ PBTBCM_CONTEXT Ctx, _In_reads_(HcdLen) const UCHAR *Hcd, _In_ ULONG HcdLen)
{
    LARGE_INTEGER delay;
    ULONG elapsedMs = 0;
    NTSTATUS status;

    /* 1. power the chip (BT_REG_ON high) + settle */
    if (Ctx->HasGpio) {
        (void)BtBcmGpioWrite(Ctx, TRUE);
    }
    delay.QuadPart = -100 * 10000;  /* 100 ms */
    KeDelayExecutionThread(KernelMode, FALSE, &delay);

    /* 2. UART at the reset baud (115200) */
    status = BtBcmUartSetBaud(Ctx, Ctx->InitBaud);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    /* 3. arm the RX reassembler and kick the state machine (sends HCI_Reset) */
    H4RxInit(&Ctx->Rx, BtBcmInitRxCb, Ctx);
    BtBcmInitStart(&Ctx->Bti, Ctx->OperBaud, Ctx->BdAddr, Hcd, HcdLen, BtBcmUartTx, Ctx);

    /* 4. read/feed until the machine finishes (sends happen from the RX callback) */
    while (!Ctx->Bti.Done && !Ctx->Bti.Error && elapsedMs < 20000) {
        UCHAR rb[256];
        ULONG got = 0;
        status = BtBcmUartReadSync(Ctx, rb, sizeof(rb), 500, &got);
        if (NT_SUCCESS(status) && got != 0) {
            H4RxFeed(&Ctx->Rx, rb, got);
        } else {
            elapsedMs += 500;   /* timeout slice (firmware download is slow @115200) */
        }
    }
    if (Ctx->Bti.Error || !Ctx->Bti.Done) {
        return STATUS_DEVICE_NOT_READY;
    }

    /* 5. switch the host UART to the operational baud (chip already switched) */
    (void)BtBcmUartPurge(Ctx);
    status = BtBcmUartSetBaud(Ctx, Ctx->OperBaud);
    delay.QuadPart = -10 * 10000;   /* 10 ms settle */
    KeDelayExecutionThread(KernelMode, FALSE, &delay);
    return status;
}

/* ---- B5: load the BCM .hcd firmware blob (PASSIVE_LEVEL) ---- */
NTSTATUS
BtBcmLoadFirmware(_Inout_ PBTBCM_CONTEXT Ctx, _In_ PCWSTR Path)
{
    UNICODE_STRING            name;
    OBJECT_ATTRIBUTES         oa;
    IO_STATUS_BLOCK           iosb;
    FILE_STANDARD_INFORMATION fsi;
    HANDLE                    h = NULL;
    PUCHAR                    buf;
    ULONG                     size;
    NTSTATUS                  status;

    RtlInitUnicodeString(&name, Path);
    InitializeObjectAttributes(&oa, &name,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    status = ZwCreateFile(&h, GENERIC_READ, &oa, &iosb, NULL, FILE_ATTRIBUTE_NORMAL,
                          FILE_SHARE_READ, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = ZwQueryInformationFile(h, &iosb, &fsi, sizeof(fsi), FileStandardInformation);
    if (!NT_SUCCESS(status) || fsi.EndOfFile.HighPart != 0 || fsi.EndOfFile.LowPart == 0) {
        ZwClose(h);
        return NT_SUCCESS(status) ? STATUS_INVALID_PARAMETER : status;
    }
    size = fsi.EndOfFile.LowPart;
    buf  = (PUCHAR)ExAllocatePool2(POOL_FLAG_NON_PAGED, size, BTBCM_POOLTAG);
    if (buf == NULL) {
        ZwClose(h);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    status = ZwReadFile(h, NULL, NULL, NULL, &iosb, buf, size, NULL, NULL);
    ZwClose(h);
    if (!NT_SUCCESS(status)) {
        ExFreePoolWithTag(buf, BTBCM_POOLTAG);
        return status;
    }
    Ctx->Hcd    = buf;
    Ctx->HcdLen = (ULONG)iosb.Information;
    return STATUS_SUCCESS;
}

VOID
BtBcmFreeFirmware(_Inout_ PBTBCM_CONTEXT Ctx)
{
    if (Ctx->Hcd != NULL) {
        ExFreePoolWithTag(Ctx->Hcd, BTBCM_POOLTAG);
        Ctx->Hcd    = NULL;
        Ctx->HcdLen = 0;
    }
}

/* ---- B3: operational async RX read pump ---- */
static EVT_WDF_REQUEST_COMPLETION_ROUTINE BtBcmRxComplete;

/* runtime RX: a fully reassembled HCI packet from the chip. Final integration
   forwards this to the upper edge (inbox BthUart / a BTHX shim); placeholder. */
static VOID
BtBcmRuntimeRxCb(PVOID Context, const UCHAR *Packet, ULONG Length)
{
    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(Packet);
    UNREFERENCED_PARAMETER(Length);
    /* TODO (B4): hand the HCI packet up to BthUart/bthport. */
}

static NTSTATUS
BtBcmSubmitRead(_Inout_ PBTBCM_CONTEXT Ctx)
{
    WDF_REQUEST_REUSE_PARAMS reuse;
    NTSTATUS status;

    WDF_REQUEST_REUSE_PARAMS_INIT(&reuse, WDF_REQUEST_REUSE_NO_FLAGS, STATUS_SUCCESS);
    WdfRequestReuse(Ctx->RxRequest, &reuse);

    status = WdfIoTargetFormatRequestForRead(Ctx->UartTarget, Ctx->RxRequest,
                                             Ctx->RxMem, NULL, NULL);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    WdfRequestSetCompletionRoutine(Ctx->RxRequest, BtBcmRxComplete, Ctx);
    if (WdfRequestSend(Ctx->RxRequest, Ctx->UartTarget, WDF_NO_SEND_OPTIONS) == FALSE) {
        return WdfRequestGetStatus(Ctx->RxRequest);
    }
    return STATUS_SUCCESS;
}

static VOID
BtBcmRxComplete(WDFREQUEST Request, WDFIOTARGET Target,
                PWDF_REQUEST_COMPLETION_PARAMS Params, WDFCONTEXT Context)
{
    PBTBCM_CONTEXT Ctx = (PBTBCM_CONTEXT)Context;
    ULONG_PTR got = Params->IoStatus.Information;
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Target);

    if (NT_SUCCESS(Params->IoStatus.Status) && got != 0) {
        H4RxFeed(&Ctx->Rx, Ctx->RxBuf, (ULONG)got);
    }
    if (Ctx->Running) {
        (void)BtBcmSubmitRead(Ctx);   /* re-arm the continuous read */
    }
}

NTSTATUS
BtBcmStartRxPump(_Inout_ PBTBCM_CONTEXT Ctx)
{
    WDF_OBJECT_ATTRIBUTES attr;
    NTSTATUS status;

    WDF_OBJECT_ATTRIBUTES_INIT(&attr);
    attr.ParentObject = Ctx->Device;
    status = WdfRequestCreate(&attr, Ctx->UartTarget, &Ctx->RxRequest);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = WdfMemoryCreatePreallocated(WDF_NO_OBJECT_ATTRIBUTES, Ctx->RxBuf,
                                         sizeof(Ctx->RxBuf), &Ctx->RxMem);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    /* switch the RX reassembler to the runtime handler (bring-up is done) */
    H4RxInit(&Ctx->Rx, BtBcmRuntimeRxCb, Ctx);
    Ctx->Running = TRUE;
    return BtBcmSubmitRead(Ctx);
}
