/*++
Module Name: gem_regio.h
Abstract:    Register-access shim for the Cadence GEM HAL (kernel MMIO vs x64
             simulation). Define GEM_SIM for the user-mode test build.
--*/
#pragma once
#ifdef GEM_SIM
extern unsigned char *g_gemBase;
unsigned int GemSimRd(unsigned off);
void         GemSimWr(unsigned off, unsigned val);
#define RD32(b,o)   GemSimRd((unsigned)(((unsigned char *)(b)+(o)) - g_gemBase))
#define WR32(b,o,v) GemSimWr((unsigned)(((unsigned char *)(b)+(o)) - g_gemBase),(unsigned)(v))
#else
#include <ntddk.h>
#define RD32(b,o)   READ_REGISTER_ULONG ((volatile ULONG *)((PUCHAR)(b)+(o)))
#define WR32(b,o,v) WRITE_REGISTER_ULONG((volatile ULONG *)((PUCHAR)(b)+(o)),(ULONG)(v))
#endif
