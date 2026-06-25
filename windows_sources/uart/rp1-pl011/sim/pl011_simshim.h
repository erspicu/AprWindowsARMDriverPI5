/*++ Minimal shim for the PL011 HAL x64 test (PL011_SIM). --*/
#pragma once
#include <stdint.h>
#include <string.h>
#include <stddef.h>
typedef uint16_t USHORT; typedef unsigned char UCHAR, *PUCHAR;
typedef uint32_t ULONG; typedef void *PVOID;
#define VOID void
#ifndef _In_
#define _In_
#endif
#ifndef _Out_
#define _Out_
#endif
