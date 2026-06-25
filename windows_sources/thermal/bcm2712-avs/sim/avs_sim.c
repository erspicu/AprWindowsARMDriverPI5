/*++ x64 sim of the BCM2712 AVS thermal logic. Build: cl /DAVS_SIM /I.. avs_sim.c ..\avs_hw.c --*/
#include <stdio.h>
#include "../avs_hw.h"

static int g_pass, g_fail;
static void check(const char *w, int c)
{ if (c) { g_pass++; printf("  [PASS] %s\n", w); } else { g_fail++; printf("  [FAIL] %s\n", w); } }

int main(void)
{
    ULONG raw;
    int mC;

    printf("== BCM2712 AVS thermal simulation ==\n");

    /* valid bits = BIT(16)|BIT(10) */
    check("both valid bits set -> valid", AvsValid((1u<<16)|(1u<<10)));
    check("missing bit10 -> invalid", !AvsValid(1u<<16));
    check("missing bit16 -> invalid", !AvsValid(1u<<10));

    /* raw extraction = low 10 bits (status with valid bits + raw 727) */
    raw = AvsRawValue((1u<<16)|(1u<<10)|727u);
    check("raw == 727", raw == 727);

    /* Pi5 formula: temp = -550*raw + 450000 (mC) */
    mC = AvsRawToMilliC(727, AVS_PI5_SLOPE, AVS_PI5_OFFSET);
    check("raw 727 -> 50150 mC (~50C)", mC == 50150);
    mC = AvsRawToMilliC(0, AVS_PI5_SLOPE, AVS_PI5_OFFSET);
    check("raw 0 -> 450000 mC (floor)", mC == 450000);
    /* ~80C: raw where -550*raw+450000=80000 -> raw=672 -> 450000-369600=80400 */
    mC = AvsRawToMilliC(672, AVS_PI5_SLOPE, AVS_PI5_OFFSET);
    check("raw 672 -> 80400 mC (~80C)", mC == 80400);

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
