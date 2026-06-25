/*++
Module Name: cec.h
Abstract:    HDMI-CEC (Consumer Electronics Control) message logic for the vc4
             HDMI CEC block. A CEC frame is: msg[0] = (initiator<<4)|destination,
             msg[1] = opcode, msg[2..] = operands (max 16 bytes total).
             Format + opcodes from the Linux source (uapi/linux/cec.h).
             OS-independent pure logic; x64-sim verified (CEC_SIM).
--*/
#pragma once

#ifdef CEC_SIM
#include "sim/cec_simshim.h"
#else
#include <ntddk.h>
#endif

#define CEC_MAX_MSG_SIZE        16u
#define CEC_LOG_ADDR_BROADCAST  0xFu

/* common opcodes (uapi/linux/cec.h) */
#define CEC_MSG_IMAGE_VIEW_ON         0x04u
#define CEC_MSG_STANDBY               0x36u
#define CEC_MSG_ACTIVE_SOURCE         0x82u   /* broadcast; operand = phys addr */
#define CEC_MSG_GIVE_PHYSICAL_ADDR    0x83u
#define CEC_MSG_REPORT_PHYSICAL_ADDR  0x84u

UCHAR CecMsgInitiator(_In_reads_(1) const UCHAR *Msg);
UCHAR CecMsgDestination(_In_reads_(1) const UCHAR *Msg);
int   CecMsgIsBroadcast(_In_reads_(1) const UCHAR *Msg);

/* assemble a CEC message: header + opcode + operands. Returns total length
   (>=2), or 0 on overflow / bad logical address. */
ULONG CecBuildMsg(_Out_writes_(Cap) UCHAR *Out, _In_ ULONG Cap,
                  _In_ UCHAR Initiator, _In_ UCHAR Destination, _In_ UCHAR Opcode,
                  _In_reads_opt_(OpLen) const UCHAR *Operands, _In_ ULONG OpLen);
