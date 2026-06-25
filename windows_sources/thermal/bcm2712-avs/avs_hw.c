/*++
Module Name: avs_hw.c
Abstract:    BCM2712 AVS thermal sensor logic. See avs_hw.h.
--*/
#include "avs_hw.h"

int
AvsValid(_In_ ULONG StatusReg)
{
    return ((StatusReg & AVS_TEMP_VALID_MASK) == AVS_TEMP_VALID_MASK) ? 1 : 0;
}

ULONG
AvsRawValue(_In_ ULONG StatusReg)
{
    return StatusReg & AVS_TEMP_DATA_MASK;
}

int
AvsRawToMilliC(_In_ ULONG Raw, _In_ int Slope, _In_ int Offset)
{
    return Slope * (int)Raw + Offset;
}
