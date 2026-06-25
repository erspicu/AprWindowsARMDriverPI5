/*++
Module Name: pmic.c
Abstract:    DA9090 PMIC linear regulator codec + on-key decode. See pmic.h.
--*/
#include "pmic.h"

ULONG
RegLinVoltageToSel(_In_ int TargetUV, _In_ int MinUV, _In_ int StepUV,
                   _In_ ULONG NSel, _In_ ULONG LinMinSel)
{
    ULONG sel;

    if (StepUV <= 0 || TargetUV < MinUV) {
        return PMIC_VSEL_INVALID;
    }
    /* DIV_ROUND_UP(TargetUV - MinUV, StepUV) + linear_min_sel */
    sel = (ULONG)(((TargetUV - MinUV) + (StepUV - 1)) / StepUV) + LinMinSel;
    if (sel >= NSel) {
        return PMIC_VSEL_INVALID;
    }
    return sel;
}

int
RegLinSelToVoltage(_In_ ULONG Sel, _In_ int MinUV, _In_ int StepUV, _In_ ULONG LinMinSel)
{
    if (Sel < LinMinSel) {
        return 0;
    }
    return MinUV + (int)(Sel - LinMinSel) * StepUV;
}

int
PmicOnkeyIsDown(_In_ ULONG StatusReg, _In_ ULONG Mask, _In_ int ActiveLow)
{
    int bitSet = (StatusReg & Mask) ? 1 : 0;
    return ActiveLow ? (bitSet ? 0 : 1) : bitSet;
}
