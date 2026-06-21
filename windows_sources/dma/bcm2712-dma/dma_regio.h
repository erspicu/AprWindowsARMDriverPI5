/*++ dma_regio.h - register shim. Define DMA_SIM for the x64 simulation. --*/
#pragma once
#ifdef DMA_SIM
extern unsigned char *g_dmaBase;
unsigned int DmaSimRd(unsigned off);
void         DmaSimWr(unsigned off, unsigned val);
#define RD32(b,o)   DmaSimRd((unsigned)(((unsigned char *)(b)+(o)) - g_dmaBase))
#define WR32(b,o,v) DmaSimWr((unsigned)(((unsigned char *)(b)+(o)) - g_dmaBase),(unsigned)(v))
#else
#include <ntddk.h>
#define RD32(b,o)   READ_REGISTER_ULONG ((volatile ULONG *)((PUCHAR)(b)+(o)))
#define WR32(b,o,v) WRITE_REGISTER_ULONG((volatile ULONG *)((PUCHAR)(b)+(o)),(ULONG)(v))
#endif
