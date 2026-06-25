/*++
Module Name: cec.c
Abstract:    HDMI-CEC message build/parse. See cec.h.
--*/
#include "cec.h"

UCHAR CecMsgInitiator(_In_reads_(1) const UCHAR *Msg)   { return (UCHAR)(Msg[0] >> 4); }
UCHAR CecMsgDestination(_In_reads_(1) const UCHAR *Msg) { return (UCHAR)(Msg[0] & 0xFu); }
int   CecMsgIsBroadcast(_In_reads_(1) const UCHAR *Msg) { return (Msg[0] & 0xFu) == CEC_LOG_ADDR_BROADCAST; }

ULONG
CecBuildMsg(_Out_writes_(Cap) UCHAR *Out, _In_ ULONG Cap,
            _In_ UCHAR Initiator, _In_ UCHAR Destination, _In_ UCHAR Opcode,
            _In_reads_opt_(OpLen) const UCHAR *Operands, _In_ ULONG OpLen)
{
    ULONG len = 2u + OpLen;   /* header + opcode + operands */
    ULONG i;

    if (Initiator > 0xFu || Destination > 0xFu) {
        return 0;
    }
    if (len > Cap || len > CEC_MAX_MSG_SIZE) {
        return 0;
    }
    Out[0] = (UCHAR)((Initiator << 4) | (Destination & 0xFu));
    Out[1] = Opcode;
    for (i = 0; i < OpLen; i++) {
        Out[2 + i] = (Operands != 0) ? Operands[i] : 0;
    }
    return len;
}
