/*++
Module Name: avs_hw.h
Abstract:    BCM2712 thermal (AVS RO temperature sensor) logic. Converts the raw
             sensor reading to milli-Celsius. Formula + Pi5 coefficients from the
             Linux source: temp = slope*raw + offset; bcm2712-ds.dtsi
             coefficients = <(-550) 450000> (NB: differs from Pi4's -487/410040).
             Valid mask = BIT(16)|BIT(10); data = low 10 bits.
             OS-independent pure logic; x64-sim verified (AVS_SIM).
--*/
#pragma once

#ifdef AVS_SIM
#include "sim/avs_simshim.h"
#else
#include <ntddk.h>
#endif

#define AVS_TEMP_DATA_MASK    0x3FFu
#define AVS_TEMP_VALID_MASK   ((1u << 16) | (1u << 10))

/* Pi5 (BCM2712) thermal-zone coefficients (milli-Celsius) */
#define AVS_PI5_SLOPE         (-550)
#define AVS_PI5_OFFSET        450000

/* is the temperature status register reading valid? */
int AvsValid(_In_ ULONG StatusReg);

/* extract the 10-bit raw sensor value from the status register */
ULONG AvsRawValue(_In_ ULONG StatusReg);

/* raw -> milli-Celsius: slope*raw + offset (use AVS_PI5_SLOPE/OFFSET) */
int AvsRawToMilliC(_In_ ULONG Raw, _In_ int Slope, _In_ int Offset);
