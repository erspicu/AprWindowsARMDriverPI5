/*++
Module Name: pmic.h
Abstract:    Pi5 PMIC (Renesas DA9090 "McLaren") helper logic for a Windows power
             driver: the linear regulator voltage<->vsel codec (universal to the
             DA90xx bucks/LDOs) and the on-key (power button) event decode.
             Formula from the Linux regulator core (regulator_map/list_voltage
             _linear). Representative table = DA9062 buck (min 300mV, step 10mV);
             the DA9090's exact per-rail tables need its datasheet/firmware.
             OS-independent pure logic; x64-sim verified (PMIC_SIM).
--*/
#pragma once

#ifdef PMIC_SIM
#include "sim/pmic_simshim.h"
#else
#include <ntddk.h>
#endif

#define PMIC_VSEL_INVALID  0xFFFFFFFFu

/* representative DA9062-style buck range (microvolts) */
#define DA9062_BUCK_MIN_UV   300000
#define DA9062_BUCK_STEP_UV  10000
#define DA9062_BUCK_NSEL     128u
#define DA9062_LINEAR_MIN    0u

/* voltage (uV) -> vsel (round up to the next representable step), linear:
   sel = linMinSel + ceil((uV - minUV)/stepUV). Returns PMIC_VSEL_INVALID if the
   request is below minUV or above the last selector. */
ULONG RegLinVoltageToSel(_In_ int TargetUV, _In_ int MinUV, _In_ int StepUV,
                         _In_ ULONG NSel, _In_ ULONG LinMinSel);

/* vsel -> voltage (uV): minUV + (sel - linMinSel)*stepUV (0 if sel < linMinSel) */
int RegLinSelToVoltage(_In_ ULONG Sel, _In_ int MinUV, _In_ int StepUV,
                       _In_ ULONG LinMinSel);

/* on-key (power button) state from a PMIC status register bit. ActiveLow=1 for
   nONKEY-style (0 = pressed); returns 1 if the button is currently down. */
int PmicOnkeyIsDown(_In_ ULONG StatusReg, _In_ ULONG Mask, _In_ int ActiveLow);
