/*++
Module Name: hci.c
Abstract:    H4 HCI framing + BCM43438 bring-up sequence (ported from hci_bcm.c).
--*/
#include "common.h"

/*
 * Frame an H4 HCI command: [H4_CMD][opcode LE16][param_len][params...].
 * Returns total bytes written, or 0 if the buffer is too small.
 */
ULONG
BtBcmBuildCommand(_Out_writes_(BufLen) PUCHAR Buf, _In_ ULONG BufLen,
                  _In_ USHORT Opcode, _In_reads_opt_(ParamLen) PUCHAR Params,
                  _In_ UCHAR ParamLen)
{
    ULONG           need = 1 + sizeof(HCI_COMMAND_HDR) + ParamLen;
    PHCI_COMMAND_HDR hdr;

    if (BufLen < need) {
        return 0;
    }
    Buf[0] = H4_CMD;
    hdr = (PHCI_COMMAND_HDR)(Buf + 1);
    hdr->Opcode   = Opcode;          /* x86/ARM little-endian == HCI wire order */
    hdr->ParamLen = ParamLen;
    if (ParamLen != 0 && Params != NULL) {
        RtlCopyMemory(Buf + 1 + sizeof(HCI_COMMAND_HDR), Params, ParamLen);
    }
    return need;
}

/*
 * BCM43438 bring-up order (port of bcm_setup): HCI reset -> enter download mode
 * -> stream firmware patch (.hcd) blocks -> launch -> raise baud -> set BD addr.
 * The .hcd blob + real UART I/O + per-step event waits need the device on HW.
 */
typedef struct _BCM_INIT_STEP { USHORT Opcode; PCSTR Desc; } BCM_INIT_STEP;

static const BCM_INIT_STEP g_BcmInit[] = {
    { HCI_OP_RESET,            "HCI_Reset" },
    { BCM_OP_DOWNLOAD_MINIDRV, "download minidriver (enter patchram)" },
    { BCM_OP_WRITE_RAM,        "write firmware (.hcd) blocks" },
    { BCM_OP_LAUNCH_RAM,       "launch RAM (run firmware)" },
    { BCM_OP_SET_UART_CLOCK,   "set UART clock" },
    { BCM_OP_SET_BAUD_RATE,    "set operational baud rate" },
    { BCM_OP_SET_BDADDR,       "set BD_ADDR" },
};

ULONG BtBcmInitStepCount(void)
{
    return (ULONG)ARRAYSIZE(g_BcmInit);
}

VOID
BtBcmBuildBaudRatePayload(_In_ ULONG Baud, _Out_writes_(6) PUCHAR Out6)
{
    Out6[0] = 0x00;                       /* encoded-rate flag = 0 (use literal) */
    Out6[1] = 0x00;
    Out6[2] = (UCHAR)(Baud & 0xFF);       /* baud, little-endian 32-bit */
    Out6[3] = (UCHAR)((Baud >> 8)  & 0xFF);
    Out6[4] = (UCHAR)((Baud >> 16) & 0xFF);
    Out6[5] = (UCHAR)((Baud >> 24) & 0xFF);
}

VOID
BtBcmBuildBdAddrPayload(_In_reads_(6) const UCHAR *Mac, _Out_writes_(6) PUCHAR Out6)
{
    /* Write_BD_ADDR wants the address least-significant byte first */
    Out6[0] = Mac[5];
    Out6[1] = Mac[4];
    Out6[2] = Mac[3];
    Out6[3] = Mac[2];
    Out6[4] = Mac[1];
    Out6[5] = Mac[0];
}

ULONG
BtBcmHcdNextCommand(_In_reads_(HcdLen) const UCHAR *Hcd, _In_ ULONG HcdLen,
                    _In_ ULONG Offset, _Out_writes_(OutCap) PUCHAR OutH4,
                    _In_ ULONG OutCap, _Out_ PULONG Consumed)
{
    USHORT opcode;
    UCHAR  paramLen;
    ULONG  frame;

    *Consumed = 0;

    /* need at least the 3-byte record header (opcode LE16 + len) */
    if (Offset + 3 > HcdLen) {
        return 0;                         /* no more records */
    }
    opcode   = (USHORT)Hcd[Offset] | ((USHORT)Hcd[Offset + 1] << 8);
    paramLen = Hcd[Offset + 2];

    if (Offset + 3 + (ULONG)paramLen > HcdLen) {
        return 0;                         /* truncated record */
    }

    frame = BtBcmBuildCommand(OutH4, OutCap, opcode,
                              (PUCHAR)(Hcd + Offset + 3), paramLen);
    if (frame == 0) {
        return 0;                         /* output buffer too small */
    }
    *Consumed = 3u + (ULONG)paramLen;
    return frame;
}
