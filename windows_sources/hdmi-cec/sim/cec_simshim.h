/*++ Minimal shim for the HDMI-CEC logic x64 test (CEC_SIM). --*/
#pragma once
#include <stdint.h>
typedef uint16_t USHORT; typedef unsigned char UCHAR, *PUCHAR;
typedef uint32_t ULONG; typedef void *PVOID;
#define VOID void
#ifndef _In_
#define _In_
#endif
#ifndef _Out_
#define _Out_
#endif
#ifndef _In_reads_
#define _In_reads_(x)
#endif
#ifndef _In_reads_opt_
#define _In_reads_opt_(x)
#endif
#ifndef _Out_writes_
#define _Out_writes_(x)
#endif
