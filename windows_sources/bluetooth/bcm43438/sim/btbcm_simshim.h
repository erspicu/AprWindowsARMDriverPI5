/*++
Module Name: btbcm_simshim.h
Abstract:    Minimal shim so the BT HCI framing logic (hci.c) compiles + runs in
             a user-mode x64 test. Used only when BTBCM_SIM is defined; the
             driver uses <ntddk.h>/<wdf.h>.
--*/
#pragma once
#include <stdint.h>
#include <string.h>
#include <stddef.h>

typedef uint16_t       USHORT;
typedef unsigned char  UCHAR, *PUCHAR;
typedef uint32_t       ULONG;
typedef const char    *PCSTR;
typedef void          *PVOID;
#define VOID void

#ifndef _In_
#define _In_
#endif
#ifndef _In_opt_
#define _In_opt_
#endif
#ifndef _Out_
#define _Out_
#endif
#ifndef _Out_opt_
#define _Out_opt_
#endif
#ifndef _Inout_
#define _Inout_
#endif
#ifndef _Out_writes_
#define _Out_writes_(x)
#endif
#ifndef _In_reads_
#define _In_reads_(x)
#endif
#ifndef _In_reads_opt_
#define _In_reads_opt_(x)
#endif

#define RtlCopyMemory(d,s,n)  memcpy((d),(s),(n))
#define RtlZeroMemory(d,n)    memset((d),0,(n))
#ifndef ARRAYSIZE
#define ARRAYSIZE(a)          (sizeof(a) / sizeof((a)[0]))
#endif
