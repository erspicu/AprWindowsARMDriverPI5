/*++ adc_regio.h - register shim. Define ADC_SIM for the x64 simulation. --*/
#pragma once
#ifdef ADC_SIM
extern unsigned char *g_adcBase;
unsigned int AdcSimRd(unsigned off);
void         AdcSimWr(unsigned off, unsigned val);
#define RD32(b,o)   AdcSimRd((unsigned)(((unsigned char *)(b)+(o)) - g_adcBase))
#define WR32(b,o,v) AdcSimWr((unsigned)(((unsigned char *)(b)+(o)) - g_adcBase),(unsigned)(v))
#else
#include <ntddk.h>
#define RD32(b,o)   READ_REGISTER_ULONG ((volatile ULONG *)((PUCHAR)(b)+(o)))
#define WR32(b,o,v) WRITE_REGISTER_ULONG((volatile ULONG *)((PUCHAR)(b)+(o)),(ULONG)(v))
#endif
