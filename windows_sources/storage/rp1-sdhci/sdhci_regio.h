/*++
Module Name: sdhci_regio.h
Abstract:    Register-access shim so the SDHCI command engine (sdhci_hw.c) is
             shared between the kernel driver (ARM64, MMIO) and the x64
             simulation harness (mock register array).
             Define SDHCI_SIM for the user-mode simulation build.
--*/
#pragma once

#ifdef SDHCI_SIM
/* user-mode simulation backend (provided by sdhci_sim.c) */
unsigned int SimRd(void *base, unsigned off, int width);
void         SimWr(void *base, unsigned off, int width, unsigned val);
#define RD8(b,o)    ((unsigned char) SimRd((b),(o),1))
#define RD16(b,o)   ((unsigned short)SimRd((b),(o),2))
#define RD32(b,o)   ((unsigned int)  SimRd((b),(o),4))
#define WR8(b,o,v)  SimWr((b),(o),1,(unsigned)(v))
#define WR16(b,o,v) SimWr((b),(o),2,(unsigned)(v))
#define WR32(b,o,v) SimWr((b),(o),4,(unsigned)(v))
#else
/* kernel driver backend (MMIO via HAL register intrinsics) */
#include <ntddk.h>
#define RD8(b,o)    READ_REGISTER_UCHAR ((volatile UCHAR  *)((PUCHAR)(b)+(o)))
#define RD16(b,o)   READ_REGISTER_USHORT((volatile USHORT *)((PUCHAR)(b)+(o)))
#define RD32(b,o)   READ_REGISTER_ULONG ((volatile ULONG  *)((PUCHAR)(b)+(o)))
#define WR8(b,o,v)  WRITE_REGISTER_UCHAR ((volatile UCHAR  *)((PUCHAR)(b)+(o)),(UCHAR)(v))
#define WR16(b,o,v) WRITE_REGISTER_USHORT((volatile USHORT *)((PUCHAR)(b)+(o)),(USHORT)(v))
#define WR32(b,o,v) WRITE_REGISTER_ULONG ((volatile ULONG  *)((PUCHAR)(b)+(o)),(ULONG)(v))
#endif
