/*++ x64 sim of the DA9090 PMIC regulator codec + on-key. Build: cl /DPMIC_SIM /I.. pmic_sim.c ..\pmic.c --*/
#include <stdio.h>
#include "../pmic.h"

static int g_pass, g_fail;
static void check(const char *w, int c)
{ if (c) { g_pass++; printf("  [PASS] %s\n", w); } else { g_fail++; printf("  [FAIL] %s\n", w); } }

int main(void)
{
    ULONG sel;

    printf("== DA9090 PMIC simulation ==\n");

    printf("-- linear regulator codec (DA9062 buck: 300mV, 10mV step) --\n");
    /* 1.000 V -> sel 70 */
    sel = RegLinVoltageToSel(1000000, DA9062_BUCK_MIN_UV, DA9062_BUCK_STEP_UV,
                             DA9062_BUCK_NSEL, DA9062_LINEAR_MIN);
    check("1.000V -> sel 70", sel == 70);
    check("sel 70 -> 1.000V", RegLinSelToVoltage(70, DA9062_BUCK_MIN_UV, DA9062_BUCK_STEP_UV, DA9062_LINEAR_MIN) == 1000000);
    /* min */
    sel = RegLinVoltageToSel(300000, DA9062_BUCK_MIN_UV, DA9062_BUCK_STEP_UV, DA9062_BUCK_NSEL, DA9062_LINEAR_MIN);
    check("min 0.300V -> sel 0", sel == 0);
    /* unaligned request rounds UP to next step: 805mV -> sel 51 (810mV) */
    sel = RegLinVoltageToSel(805000, DA9062_BUCK_MIN_UV, DA9062_BUCK_STEP_UV, DA9062_BUCK_NSEL, DA9062_LINEAR_MIN);
    check("805mV rounds up -> sel 51", sel == 51);
    check("sel 51 -> 810mV", RegLinSelToVoltage(51, DA9062_BUCK_MIN_UV, DA9062_BUCK_STEP_UV, DA9062_LINEAR_MIN) == 810000);
    /* below min / above range -> invalid */
    check("below min -> invalid", RegLinVoltageToSel(200000, DA9062_BUCK_MIN_UV, DA9062_BUCK_STEP_UV, DA9062_BUCK_NSEL, DA9062_LINEAR_MIN) == PMIC_VSEL_INVALID);
    check("above range -> invalid", RegLinVoltageToSel(99000000, DA9062_BUCK_MIN_UV, DA9062_BUCK_STEP_UV, DA9062_BUCK_NSEL, DA9062_LINEAR_MIN) == PMIC_VSEL_INVALID);

    printf("-- on-key (power button) --\n");
    /* nONKEY active-low: bit clear = pressed */
    check("nONKEY clear -> down", PmicOnkeyIsDown(0x00, 0x01, 1) == 1);
    check("nONKEY set -> up", PmicOnkeyIsDown(0x01, 0x01, 1) == 0);
    /* active-high event bit */
    check("event bit set -> down", PmicOnkeyIsDown(0x04, 0x04, 0) == 1);

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
