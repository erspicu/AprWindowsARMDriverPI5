/*++
Module Name: dwspi_simshim.h
Abstract:    "fake kernel" shim so the DW SSI HAL (dw_spi_hw.c) compiles + runs
             in a user-mode x64 test, with register access redirected to a mock.
             Used only when DWSPI_SIM is defined; the driver uses <ntddk.h>.
--*/
#pragma once
#include <stdint.h>
#include <string.h>

typedef uint32_t       UINT32;
typedef uint16_t       UINT16;
typedef uint8_t        UINT8;
typedef unsigned char  UCHAR, *PUCHAR;
typedef uint32_t       ULONG;
typedef unsigned char  BOOLEAN;
typedef int32_t        NTSTATUS;
typedef uint64_t       SIZE_T;
#define VOID void
#define FORCEINLINE static __inline

#ifndef _In_
#define _In_
#endif
#ifndef _Out_
#define _Out_
#endif
#ifndef _Inout_
#define _Inout_
#endif
#ifndef _In_reads_opt_
#define _In_reads_opt_(x)
#endif
#ifndef _Out_writes_opt_
#define _Out_writes_opt_(x)
#endif

#define TRUE  1
#define FALSE 0

#define STATUS_SUCCESS                      ((NTSTATUS)0x00000000)
#define STATUS_IO_TIMEOUT                   ((NTSTATUS)0xC00000B5)
#define STATUS_DEVICE_CONFIGURATION_ERROR   ((NTSTATUS)0xC0000182)
#define NT_SUCCESS(s)                       (((NTSTATUS)(s)) >= 0)

#define RtlZeroMemory(d,n)              memset((d),0,(n))
#define KeStallExecutionProcessor(us)   ((void)0)

extern unsigned char *g_spiBase;
unsigned int SpiSimRd(unsigned off);
void         SpiSimWr(unsigned off, unsigned val);
#define READ_REGISTER_ULONG(p)     SpiSimRd((unsigned)((unsigned char *)(p) - g_spiBase))
#define WRITE_REGISTER_ULONG(p,v)  SpiSimWr((unsigned)((unsigned char *)(p) - g_spiBase),(unsigned)(v))
