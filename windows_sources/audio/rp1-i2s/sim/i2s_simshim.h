/*++
Module Name: i2s_simshim.h
Abstract:    "fake kernel" shim for the DesignWare I2S HAL x64 simulation.
             Used only when I2S_SIM is defined; the driver uses <ntddk.h>.
--*/
#pragma once
#include <stdint.h>
#include <string.h>

typedef uint32_t       UINT32;
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

#define TRUE  1
#define FALSE 0
#define STATUS_SUCCESS                     ((NTSTATUS)0x00000000)
#define STATUS_INVALID_PARAMETER           ((NTSTATUS)0xC000000D)
#define STATUS_DEVICE_CONFIGURATION_ERROR  ((NTSTATUS)0xC0000182)
#define NT_SUCCESS(s)                      (((NTSTATUS)(s)) >= 0)
#define RtlZeroMemory(d,n)                 memset((d),0,(n))

extern unsigned char *g_i2sBase;
unsigned int I2sSimRd(unsigned off);
void         I2sSimWr(unsigned off, unsigned val);
#define READ_REGISTER_ULONG(p)     I2sSimRd((unsigned)((unsigned char *)(p) - g_i2sBase))
#define WRITE_REGISTER_ULONG(p,v)  I2sSimWr((unsigned)((unsigned char *)(p) - g_i2sBase),(unsigned)(v))
