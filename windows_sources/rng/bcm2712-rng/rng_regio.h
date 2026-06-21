/*++ rng_regio.h - register shim. Define RNG_SIM for the x64 simulation. --*/
#pragma once
#ifdef RNG_SIM
extern unsigned char *g_rngBase;
unsigned int RngSimRd(unsigned off);
void         RngSimWr(unsigned off, unsigned val);
#define RD32(b,o)   RngSimRd((unsigned)(((unsigned char *)(b)+(o)) - g_rngBase))
#define WR32(b,o,v) RngSimWr((unsigned)(((unsigned char *)(b)+(o)) - g_rngBase),(unsigned)(v))
#else
#include <ntddk.h>
#define RD32(b,o)   READ_REGISTER_ULONG ((volatile ULONG *)((PUCHAR)(b)+(o)))
#define WR32(b,o,v) WRITE_REGISTER_ULONG((volatile ULONG *)((PUCHAR)(b)+(o)),(ULONG)(v))
#endif
