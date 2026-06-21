/*++ pwm_regio.h - register shim (kernel MMIO vs x64 sim). Define PWM_SIM for sim. --*/
#pragma once
#ifdef PWM_SIM
extern unsigned char *g_pwmBase;
unsigned int PwmSimRd(unsigned off);
void         PwmSimWr(unsigned off, unsigned val);
#define RD32(b,o)   PwmSimRd((unsigned)(((unsigned char *)(b)+(o)) - g_pwmBase))
#define WR32(b,o,v) PwmSimWr((unsigned)(((unsigned char *)(b)+(o)) - g_pwmBase),(unsigned)(v))
#else
#include <ntddk.h>
#define RD32(b,o)   READ_REGISTER_ULONG ((volatile ULONG *)((PUCHAR)(b)+(o)))
#define WR32(b,o,v) WRITE_REGISTER_ULONG((volatile ULONG *)((PUCHAR)(b)+(o)),(ULONG)(v))
#endif
