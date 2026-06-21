/*++ clk_regio.h - register shim. Define CLK_SIM for the x64 simulation. --*/
#pragma once
#ifdef CLK_SIM
extern unsigned char *g_clkBase;
unsigned int ClkSimRd(unsigned off);
void         ClkSimWr(unsigned off, unsigned val);
#define RD32(b,o)   ClkSimRd((unsigned)(((unsigned char *)(b)+(o)) - g_clkBase))
#define WR32(b,o,v) ClkSimWr((unsigned)(((unsigned char *)(b)+(o)) - g_clkBase),(unsigned)(v))
#else
#include <ntddk.h>
#define RD32(b,o)   READ_REGISTER_ULONG ((volatile ULONG *)((PUCHAR)(b)+(o)))
#define WR32(b,o,v) WRITE_REGISTER_ULONG((volatile ULONG *)((PUCHAR)(b)+(o)),(ULONG)(v))
#endif
