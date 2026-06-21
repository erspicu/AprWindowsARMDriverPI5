/*++ pio_regio.h - register shim. Define PIO_SIM for the x64 simulation. --*/
#pragma once
#ifdef PIO_SIM
extern unsigned char *g_pioBase;
unsigned int PioSimRd(unsigned off);
void         PioSimWr(unsigned off, unsigned val);
#define RD32(b,o)   PioSimRd((unsigned)(((unsigned char *)(b)+(o)) - g_pioBase))
#define WR32(b,o,v) PioSimWr((unsigned)(((unsigned char *)(b)+(o)) - g_pioBase),(unsigned)(v))
#else
#include <ntddk.h>
#define RD32(b,o)   READ_REGISTER_ULONG ((volatile ULONG *)((PUCHAR)(b)+(o)))
#define WR32(b,o,v) WRITE_REGISTER_ULONG((volatile ULONG *)((PUCHAR)(b)+(o)),(ULONG)(v))
#endif
