/*++ brcm_regio.h - register shim. Define BRCMGPIO_SIM for the x64 simulation. --*/
#pragma once
#ifdef BRCMGPIO_SIM
extern unsigned char *g_brcmBase;
unsigned int BrcmSimRd(unsigned off);
void         BrcmSimWr(unsigned off, unsigned val);
#define RD32(b,o)   BrcmSimRd((unsigned)(((unsigned char *)(b)+(o)) - g_brcmBase))
#define WR32(b,o,v) BrcmSimWr((unsigned)(((unsigned char *)(b)+(o)) - g_brcmBase),(unsigned)(v))
#else
#include <ntddk.h>
#define RD32(b,o)   READ_REGISTER_ULONG ((volatile ULONG *)((PUCHAR)(b)+(o)))
#define WR32(b,o,v) WRITE_REGISTER_ULONG((volatile ULONG *)((PUCHAR)(b)+(o)),(ULONG)(v))
#endif
