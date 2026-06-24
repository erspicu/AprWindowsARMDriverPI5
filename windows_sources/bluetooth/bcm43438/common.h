/*++
Module Name: common.h
Abstract:    BCM43438 Bluetooth UART (H4 HCI) transport - KMDF skeleton.
             Ports the reusable H4 HCI framing + BCM43438 bring-up sequence from
             Linux hci_bcm.c / hci_h4.c. The underlying UART is the RP1/PL011 the
             BT chip is wired to. Full integration with the Windows Bluetooth
             stack uses the bthx HCI-transport interface (bthx.h IS in the
             standard WDK under shared/; see MD/Note/bluetooth/). bthport.sys
             handles L2CAP/SDP/profiles; this driver is the H4 transport.
--*/
#pragma once

#ifdef BTBCM_SIM
#include "sim/btbcm_simshim.h"   /* x64 user-mode test shim (HCI framing only) */
#else
#include <ntddk.h>
#include <wdf.h>
#endif

#define BTBCM_POOLTAG  'cBtR'   // 'RtBc'

/* H4 UART HCI packet types (1 leading byte on the wire) */
#define H4_CMD  0x01
#define H4_ACL  0x02
#define H4_SCO  0x03
#define H4_EVT  0x04

/* BCM vendor HCI opcodes (OGF 0x3f) + core reset (from hci_bcm.c) */
#define HCI_OP_RESET             0x0C03
#define BCM_OP_SET_BDADDR        0xFC01
#define BCM_OP_SET_BAUD_RATE     0xFC18
#define BCM_OP_SET_SLEEP_PARAMS  0xFC27
#define BCM_OP_DOWNLOAD_MINIDRV  0xFC2E
#define BCM_OP_WRITE_RAM         0xFC4C
#define BCM_OP_LAUNCH_RAM        0xFC4E
#define BCM_OP_SET_UART_CLOCK    0xFC45

#pragma pack(push, 1)
typedef struct _HCI_COMMAND_HDR {
    USHORT Opcode;      /* OCF | (OGF << 10), little-endian on the wire */
    UCHAR  ParamLen;
} HCI_COMMAND_HDR, *PHCI_COMMAND_HDR;

typedef struct _HCI_EVENT_HDR {
    UCHAR EventCode;
    UCHAR ParamLen;
} HCI_EVENT_HDR, *PHCI_EVENT_HDR;
#pragma pack(pop)

#ifndef BTBCM_SIM
typedef struct _BTBCM_CONTEXT {
    WDFDEVICE   Device;
    WDFIOTARGET UartTarget;   /* the PL011 UART carrying H4 HCI */
    ULONG       InitBaud;     /* 115200 at reset */
    ULONG       OperBaud;     /* 3 Mbps operational */
} BTBCM_CONTEXT, *PBTBCM_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BTBCM_CONTEXT, BtBcmGetContext)

EVT_WDF_DRIVER_DEVICE_ADD BtBcmEvtDeviceAdd;
#endif

/* hci.c */
ULONG BtBcmBuildCommand(_Out_writes_(BufLen) PUCHAR Buf, _In_ ULONG BufLen,
                        _In_ USHORT Opcode, _In_reads_opt_(ParamLen) PUCHAR Params,
                        _In_ UCHAR ParamLen);
ULONG BtBcmInitStepCount(void);

/* Update_Baud_Rate (0xFC18) payload: 2 zero bytes + baud as LE32 -> Out6.
   (verified vs Pi5 DT max-speed = 3,000,000 -> 00 00 c0 c6 2d 00) */
VOID BtBcmBuildBaudRatePayload(_In_ ULONG Baud, _Out_writes_(6) PUCHAR Out6);

/* Write_BD_ADDR (0xFC01) payload: the 6-byte MAC in reverse byte order.
   (Pi5 DT local-bd-address is already stored in this reversed wire order) */
VOID BtBcmBuildBdAddrPayload(_In_reads_(6) const UCHAR *Mac, _Out_writes_(6) PUCHAR Out6);

/* Parse one .hcd record at Offset ([opcode LE16][len u8][data]) and frame it as
   an H4 command into OutH4. *Consumed = bytes taken from the .hcd (3+len).
   Returns the H4 frame length, or 0 when no more records / truncated / no room.
   (Pi5 BCM4345C0.hcd verified: first record opcode 0xFC4C Write_RAM, len 0x46) */
ULONG BtBcmHcdNextCommand(_In_reads_(HcdLen) const UCHAR *Hcd, _In_ ULONG HcdLen,
                          _In_ ULONG Offset, _Out_writes_(OutCap) PUCHAR OutH4,
                          _In_ ULONG OutCap, _Out_ PULONG Consumed);

/* ---- init_sm.c : BCM bring-up state machine (event-driven, HW-independent) ----
   Drives Reset -> Download_Minidriver -> (.hcd Write_RAM records) -> Launch_RAM
   -> Reset -> Write_BD_ADDR -> Update_Baud_Rate, advancing one step per matching
   HCI Command Complete. Transport is injected via BTI_TX_FN so it is x64-sim
   testable; the driver wires Tx to the UART and feeds events from the RX pump. */
typedef int (*BTI_TX_FN)(PVOID Ctx, const UCHAR *Data, ULONG Len);

typedef enum _BTI_STEP {
    BTI_RESET1 = 0, BTI_DOWNLOAD, BTI_HCD, BTI_LAUNCH,
    BTI_RESET2, BTI_SETBDADDR, BTI_SETBAUD, BTI_DONE
} BTI_STEP;

typedef struct _BTI {
    BTI_STEP     Step;
    ULONG        Baud;
    UCHAR        BdAddr[6];
    const UCHAR *Hcd;
    ULONG        HcdLen;
    ULONG        HcdOff;
    USHORT       Pending;     /* opcode we await a Command Complete for */
    BTI_TX_FN    Tx;
    PVOID        TxCtx;
    int          Done;
    int          Error;
} BTI, *PBTI;

VOID BtBcmInitStart(_Out_ PBTI S, _In_ ULONG Baud, _In_reads_(6) const UCHAR *Mac,
                    _In_reads_(HcdLen) const UCHAR *Hcd, _In_ ULONG HcdLen,
                    _In_ BTI_TX_FN Tx, _In_opt_ PVOID TxCtx);
VOID BtBcmInitOnEvent(_Inout_ PBTI S, _In_reads_(Len) const UCHAR *Pkt, _In_ ULONG Len);

/* ---- h4_parser.c : H4 UART HCI receive reassembly state machine ---- */
#define H4_RX_MAX_PAYLOAD  1024u                          /* HCI ACL/event bound */
#define H4_RX_MAX_PACKET   (1u + 4u + H4_RX_MAX_PAYLOAD)  /* type + max hdr + payload */

typedef enum _H4_RX_STATE {
    H4_WANT_TYPE = 0,
    H4_WANT_HEADER,
    H4_WANT_PAYLOAD
} H4_RX_STATE;

/* called once per fully reassembled packet: [type][header...][payload...] */
typedef VOID (*H4_RX_PACKET_CB)(PVOID Context, const UCHAR *Packet, ULONG Length);

typedef struct _H4_RX {
    H4_RX_STATE     State;
    UCHAR           Type;
    UCHAR           Hdr[4];
    ULONG           HdrNeed;
    ULONG           HdrGot;
    ULONG           PayloadNeed;
    ULONG           PayloadGot;
    UCHAR           Packet[H4_RX_MAX_PACKET];
    ULONG           PacketLen;
    H4_RX_PACKET_CB OnPacket;
    PVOID           OnPacketCtx;
} H4_RX, *PH4_RX;

VOID  H4RxInit(_Out_ PH4_RX Rx, _In_ H4_RX_PACKET_CB OnPacket, _In_opt_ PVOID CbContext);
ULONG H4RxFeed(_Inout_ PH4_RX Rx, _In_reads_(Len) const UCHAR *Data, _In_ ULONG Len);
ULONG H4IsCommandComplete(_In_reads_(Len) const UCHAR *Pkt, _In_ ULONG Len,
                          _In_ USHORT Opcode, _Out_opt_ PUCHAR Status);
