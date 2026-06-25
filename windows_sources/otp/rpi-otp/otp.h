/*++
Module Name: otp.h
Abstract:    Raspberry Pi OTP (one-time programmable memory) access logic. OTP on
             Pi is read/written via the VideoCore firmware mailbox property
             channel (raspberrypi-otp.c): the tag value buffer is
             { block, start_row, num_words, row0, row1, ... }. This builds the
             mailbox message + parses the response. OS-independent pure logic;
             x64-sim verified (OTP_SIM).
--*/
#pragma once

#ifdef OTP_SIM
#include "sim/otp_simshim.h"
#else
#include <ntddk.h>
#endif

/* firmware property tags (raspberrypi-firmware.h / raspberrypi-otp.c) */
#define RPI_TAG_GET_CUSTOMER_OTP  0x00030021u
#define RPI_TAG_SET_CUSTOMER_OTP  0x00038021u
#define RPI_TAG_GET_USER_OTP      0x00030024u
#define RPI_TAG_SET_USER_OTP      0x00038024u

/* Build an OTP read mailbox message into Buf (CapWords u32). Lays out:
   [size][req=0][tag][valBufSize][reqresp=0][block][startRow][numWords]
   [numWords zero data words][end=0]. Returns total bytes, 0 on overflow. */
ULONG OtpBuildReadMsg(_Out_writes_(CapWords) ULONG *Buf, _In_ ULONG CapWords,
                      _In_ ULONG Tag, _In_ ULONG Block, _In_ ULONG StartRow,
                      _In_ ULONG NumWords);

/* Extract OTP row data from a (firmware-filled) response message into Rows.
   Returns the number of rows copied (<= MaxRows). */
ULONG OtpParseResponse(_In_ const ULONG *Buf, _Out_writes_(MaxRows) ULONG *Rows,
                       _In_ ULONG MaxRows);
