/*++
Module Name: sdio_core.h
Abstract:    CYW43455 SDIO control-plane logic (OS/transport independent). The
             actual CMD52/CMD53 are injected via SDIO_OPS so the same logic runs
             over real sdbus.sys in the driver, or a mock bus in the x64 sim
             (CYW_SIM). Implements the backplane address-window mechanism and the
             "read Chip ID 0x4345" bring-up milestone, plus WHD NVRAM preprocess.

   NOTE: exact SBADDR register bit layout follows the brcmfmac convention; the
   precise semantics must be confirmed against Linux brcmfmac / on real hardware
   (Pi5 milestone C3). The logic here is self-consistent and x64-sim verified.
--*/
#pragma once

#ifdef CYW_SIM
#include "sim/wifi_simshim.h"
#else
#include <ntddk.h>
#endif

#define SDIO_FUNC_BACKPLANE        1u

/* CYW43455 function-1 backplane address window registers */
#define SBSDIO_FUNC1_SBADDRLOW     0x1000Au   /* window addr bits [15:8]  */
#define SBSDIO_FUNC1_SBADDRMID     0x1000Bu   /* window addr bits [23:16] */
#define SBSDIO_FUNC1_SBADDRHIGH    0x1000Cu   /* window addr bits [31:24] */
#define SBSDIO_SB_OFT_ADDR_MASK    0x7FFFu    /* 15-bit offset inside the window */
#define SBSDIO_SB_ACCESS_WINDOW    0x8000u    /* F1 region that maps to the window */

#define CYW43455_CHIPCOMMON_BASE   0x18000000u
#define CYW43455_CHIP_ID           0x4345u

/* injected transport: return 0 on success, non-zero on error */
typedef int (*SDIO_CMD52_FN)(PVOID Ctx, UCHAR Func, ULONG Addr, PUCHAR Data, int Write);
typedef int (*SDIO_CMD53_FN)(PVOID Ctx, UCHAR Func, ULONG Addr, PUCHAR Buf, ULONG Len, int Write);

typedef struct _SDIO_OPS {
    SDIO_CMD52_FN Cmd52;
    SDIO_CMD53_FN Cmd53;
    PVOID         Ctx;
} SDIO_OPS, *PSDIO_OPS;

/* set the F1 backplane window so that Addr becomes reachable at 0x8000+offset */
int SdioSetBackplaneWindow(_In_ const SDIO_OPS *Ops, _In_ ULONG Addr);

/* read a 32-bit backplane register (sets window, CMD53 reads 4 bytes) */
int SdioBackplaneRead32(_In_ const SDIO_OPS *Ops, _In_ ULONG Addr, _Out_ ULONG *Value);

/* the first bring-up milestone: read ChipCommon chip-id, expect low16 == 0x4345 */
int Cyw43455ReadChipId(_In_ const SDIO_OPS *Ops, _Out_ USHORT *ChipId);

/*
 * Convert Pi NVRAM text into the WHD-expected pool: drop blank lines and
 * '#' comment lines, terminate each remaining "key=value" line with a single
 * '\0' (not '\n'), and end the whole pool with an extra '\0' (so it ends '\0\0').
 * Returns output byte count, or 0 on overflow.
 */
ULONG WhdNvramPreprocess(_In_reads_(InLen) const char *In, _In_ ULONG InLen,
                         _Out_writes_(OutCap) PUCHAR Out, _In_ ULONG OutCap);
