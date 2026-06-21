/*++ mbox_regio.h - register shim. Define MBOX_SIM for the x64 simulation. --*/
#pragma once
#ifdef MBOX_SIM
extern unsigned char *g_mboxBase;
unsigned int MboxSimRd(unsigned off);
void         MboxSimWr(unsigned off, unsigned val);
#define RD32(b,o)   MboxSimRd((unsigned)(((unsigned char *)(b)+(o)) - g_mboxBase))
#define WR32(b,o,v) MboxSimWr((unsigned)(((unsigned char *)(b)+(o)) - g_mboxBase),(unsigned)(v))
#else
#include <ntddk.h>
#define RD32(b,o)   READ_REGISTER_ULONG ((volatile ULONG *)((PUCHAR)(b)+(o)))
#define WR32(b,o,v) WRITE_REGISTER_ULONG((volatile ULONG *)((PUCHAR)(b)+(o)),(ULONG)(v))
#endif
