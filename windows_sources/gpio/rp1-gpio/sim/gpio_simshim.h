/*++
Module Name: gpio_simshim.h
Abstract:    "fake kernel" shim for the RP1 GPIO HAL x64 simulation.
             Used only when GPIO_SIM is defined; the driver uses <ntddk.h>.
--*/
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef uint32_t       UINT32;
typedef unsigned char  UCHAR, *PUCHAR;
typedef uint32_t       ULONG;
typedef unsigned char  BOOLEAN;
typedef uint64_t       SIZE_T;
#define VOID void
#define FORCEINLINE static __inline

#ifndef _In_
#define _In_
#endif
#ifndef _Out_
#define _Out_
#endif

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif

extern unsigned char *g_gpioBase;
unsigned int GpioSimRd(unsigned off);
void         GpioSimWr(unsigned off, unsigned val);
#define READ_REGISTER_ULONG(p)     GpioSimRd((unsigned)((unsigned char *)(p) - g_gpioBase))
#define WRITE_REGISTER_ULONG(p,v)  GpioSimWr((unsigned)((unsigned char *)(p) - g_gpioBase),(unsigned)(v))
