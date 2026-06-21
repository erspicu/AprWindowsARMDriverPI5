/*++
Module Name: bcmspi_simshim.h
Abstract:    "fake kernel" shim for the BCM SPI HAL x64 simulation.
             Used only when BCMSPI_SIM is defined; the driver uses <ntddk.h>.
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
#define STATUS_SUCCESS      ((NTSTATUS)0x00000000)
#define STATUS_IO_TIMEOUT   ((NTSTATUS)0xC00000B5)
#define NT_SUCCESS(s)       (((NTSTATUS)(s)) >= 0)
#define RtlZeroMemory(d,n)              memset((d),0,(n))
#define KeStallExecutionProcessor(us)   ((void)0)

extern unsigned char *g_bspiBase;
unsigned int BspiSimRd(unsigned off);
void         BspiSimWr(unsigned off, unsigned val);
#define READ_REGISTER_ULONG(p)     BspiSimRd((unsigned)((unsigned char *)(p) - g_bspiBase))
#define WRITE_REGISTER_ULONG(p,v)  BspiSimWr((unsigned)((unsigned char *)(p) - g_bspiBase),(unsigned)(v))
