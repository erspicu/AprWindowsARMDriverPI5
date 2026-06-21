/*++
Module Name: common.h
Abstract:    BCM43438 Bluetooth UART (H4 HCI) transport - KMDF skeleton.
             Ports the reusable H4 HCI framing + BCM43438 bring-up sequence from
             Linux hci_bcm.c / hci_h4.c. The underlying UART is the RP1/PL011 the
             BT chip is wired to. Full integration with the Windows Bluetooth
             stack needs the bthx HCI-transport interface (bthx.h, not in this WDK).
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
