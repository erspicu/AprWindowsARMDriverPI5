/*++
Module Name: sdbus_glue.c
Abstract:    SDIO_OPS implementation over the inbox sdbus.sys interface
             (SDBUS_REQUEST_PACKET + SdBusSubmitRequest, <ntddsd.h>/<sddef.h>).
             See sdbus_glue.h.

   Verified against WDK 10.0.26100 headers: the modern interface uses
   RequestFunction = SDRF_DEVICE_COMMAND with a SDCMD_DESCRIPTOR (SDCMD_IO_RW_DIRECT
   = CMD52, SDCMD_IO_RW_EXTENDED = CMD53) + a raw SD command Argument we build
   here. (Older docs/examples show SDRF_READ_PORT etc. — that's the WDK8-era API.)

   SDIO command argument layout (SD spec):
     CMD52: [31]rw [30:28]func [27]raw [25:9]addr(17b) [7:0]data
     CMD53: [31]rw [30:28]func [27]block [26]incr [25:9]addr(17b) [8:0]count
--*/
#include "sdbus_glue.h"
#include <ntddsd.h>
#include <sddef.h>

static int
WifiSdioCmd52(PVOID Ctx, UCHAR Func, ULONG Addr, PUCHAR Data, int Write)
{
    PWIFI_SDIO s = (PWIFI_SDIO)Ctx;
    SDBUS_REQUEST_PACKET pkt;
    SDCMD_DESCRIPTOR desc;
    ULONG arg;
    NTSTATUS st;

    RtlZeroMemory(&pkt, sizeof(pkt));
    arg = ((ULONG)(Write ? 1u : 0u) << 31) | (((ULONG)Func & 0x7u) << 28) |
          ((Addr & 0x1FFFFu) << 9);
    if (Write) {
        arg |= ((ULONG)*Data & 0xFFu);
    }

    desc.Cmd               = SDCMD_IO_RW_DIRECT;          /* 52 */
    desc.CmdClass          = SDCC_STANDARD;
    desc.TransferDirection = Write ? SDTD_WRITE : SDTD_READ;
    desc.TransferType      = SDTT_CMD_ONLY;
    desc.ResponseType      = SDRT_5;                      /* SDIO R5 */

    pkt.RequestFunction                    = SDRF_DEVICE_COMMAND;
    pkt.Parameters.DeviceCommand.CmdDesc   = desc;
    pkt.Parameters.DeviceCommand.Argument  = arg;
    pkt.Parameters.DeviceCommand.Mdl       = NULL;
    pkt.Parameters.DeviceCommand.Length    = 0;

    st = SdBusSubmitRequest(s->InterfaceContext, &pkt);
    if (!NT_SUCCESS(st)) {
        return -1;
    }
    if (!Write) {
        *Data = (UCHAR)(pkt.ResponseData.AsULONG[0] & 0xFFu);  /* R5 data byte */
    }
    return 0;
}

static int
WifiSdioCmd53(PVOID Ctx, UCHAR Func, ULONG Addr, PUCHAR Buf, ULONG Len, int Write)
{
    PWIFI_SDIO s = (PWIFI_SDIO)Ctx;
    SDBUS_REQUEST_PACKET pkt;
    SDCMD_DESCRIPTOR desc;
    PMDL mdl;
    ULONG arg;
    NTSTATUS st;

    if (Buf == NULL || Len == 0) {
        return -1;
    }
    RtlZeroMemory(&pkt, sizeof(pkt));
    /* byte mode (bit27=0), incrementing address (bit26=1), count in [8:0] */
    arg = ((ULONG)(Write ? 1u : 0u) << 31) | (((ULONG)Func & 0x7u) << 28) |
          (1u << 26) | ((Addr & 0x1FFFFu) << 9) | (Len & 0x1FFu);

    desc.Cmd               = SDCMD_IO_RW_EXTENDED;        /* 53 */
    desc.CmdClass          = SDCC_STANDARD;
    desc.TransferDirection = Write ? SDTD_WRITE : SDTD_READ;
    desc.TransferType      = SDTT_SINGLE_BLOCK;
    desc.ResponseType      = SDRT_5;

    /* the data buffer must be NonPaged; wrap it in an MDL for the bus driver */
    mdl = IoAllocateMdl(Buf, Len, FALSE, FALSE, NULL);
    if (mdl == NULL) {
        return -1;
    }
    MmBuildMdlForNonPagedPool(mdl);

    pkt.RequestFunction                   = SDRF_DEVICE_COMMAND;
    pkt.Parameters.DeviceCommand.CmdDesc  = desc;
    pkt.Parameters.DeviceCommand.Argument = arg;
    pkt.Parameters.DeviceCommand.Mdl      = mdl;
    pkt.Parameters.DeviceCommand.Length   = Len;

    st = SdBusSubmitRequest(s->InterfaceContext, &pkt);
    IoFreeMdl(mdl);
    return NT_SUCCESS(st) ? 0 : -1;
}

NTSTATUS
WifiSdioInit(_Out_ PWIFI_SDIO Sd, _In_ WDFDEVICE Device)
{
    NTSTATUS status;

    RtlZeroMemory(Sd, sizeof(*Sd));
    Sd->SdBus.Size    = sizeof(SDBUS_INTERFACE_STANDARD);
    Sd->SdBus.Version = SDBUS_INTERFACE_VERSION;

    status = WdfFdoQueryForInterface(Device, &GUID_SDBUS_INTERFACE_STANDARD,
                                     (PINTERFACE)&Sd->SdBus,
                                     sizeof(SDBUS_INTERFACE_STANDARD),
                                     SDBUS_INTERFACE_VERSION, NULL);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    Sd->InterfaceContext = Sd->SdBus.Context;
    Sd->Ops.Cmd52 = WifiSdioCmd52;
    Sd->Ops.Cmd53 = WifiSdioCmd53;
    Sd->Ops.Ctx   = Sd;
    return STATUS_SUCCESS;
}

NTSTATUS
WifiSdioReadChipId(_In_ PWIFI_SDIO Sd, _Out_ USHORT *ChipId)
{
    return (Cyw43455ReadChipId(&Sd->Ops, ChipId) == 0)
               ? STATUS_SUCCESS : STATUS_DEVICE_NOT_READY;
}
