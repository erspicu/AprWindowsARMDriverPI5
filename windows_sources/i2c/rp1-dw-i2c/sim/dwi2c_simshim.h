/*++
Module Name: dwi2c_simshim.h
Abstract:    Minimal "fake kernel" shim so the DesignWare I2C HAL (dw_i2c_hw.c)
             compiles and runs in a user-mode x64 test, with register access
             redirected to a mock backend (provided by dwi2c_sim.c). Used only
             when DWI2C_SIM is defined; the real driver uses <ntddk.h>.
--*/
#pragma once
#include <stdint.h>
#include <string.h>

typedef uint32_t       UINT32;
typedef uint16_t       UINT16;
typedef unsigned char  UCHAR, *PUCHAR;
typedef uint32_t       ULONG;
typedef unsigned char  BOOLEAN;
typedef int32_t        NTSTATUS;
typedef uint64_t       SIZE_T;
#define VOID void
#define FORCEINLINE static __inline

/* SAL no-ops (guard against the CRT's sal.h) */
#ifndef _In_
#define _In_
#endif
#ifndef _Out_
#define _Out_
#endif
#ifndef _Inout_
#define _Inout_
#endif
#ifndef _In_reads_
#define _In_reads_(x)
#endif
#ifndef _Out_writes_
#define _Out_writes_(x)
#endif

#define TRUE  1
#define FALSE 0

#define STATUS_SUCCESS                      ((NTSTATUS)0x00000000)
#define STATUS_IO_TIMEOUT                   ((NTSTATUS)0xC00000B5)
#define STATUS_IO_DEVICE_ERROR              ((NTSTATUS)0xC0000185)
#define STATUS_DEVICE_CONFIGURATION_ERROR   ((NTSTATUS)0xC0000182)
#define NT_SUCCESS(s)                       (((NTSTATUS)(s)) >= 0)

#define RtlZeroMemory(d,n)              memset((d),0,(n))
#define KeStallExecutionProcessor(us)   ((void)0)

/* register access -> mock backend keyed by offset (ptr - base) */
extern unsigned char *g_dwBase;
unsigned int DwSimRd(unsigned off);
void         DwSimWr(unsigned off, unsigned val);
#define READ_REGISTER_ULONG(p)     DwSimRd((unsigned)((unsigned char *)(p) - g_dwBase))
#define WRITE_REGISTER_ULONG(p,v)  DwSimWr((unsigned)((unsigned char *)(p) - g_dwBase),(unsigned)(v))
