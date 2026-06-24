/*++
Module Name: wifi_simshim.h
Abstract:    Minimal shim so the CYW43455 SDIO control-plane logic (sdio_core.c)
             compiles + runs in a user-mode x64 test. Used only when CYW_SIM is
             defined; the driver uses <ntddk.h>.
--*/
#pragma once
#include <stdint.h>
#include <string.h>
#include <stddef.h>

typedef uint16_t       USHORT;
typedef unsigned char  UCHAR, *PUCHAR;
typedef uint32_t       ULONG;
typedef void          *PVOID;
#define VOID void

#ifndef _In_
#define _In_
#endif
#ifndef _In_reads_
#define _In_reads_(x)
#endif
#ifndef _Out_
#define _Out_
#endif
#ifndef _Out_writes_
#define _Out_writes_(x)
#endif
