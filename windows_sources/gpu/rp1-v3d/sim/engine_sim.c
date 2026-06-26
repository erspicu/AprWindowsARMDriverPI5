/* x64 sim of V3D submit + MMU engine. Build: cl /DV3D_ENG_SIM /I.. engine_sim.c ..\v3d_engine.c */
#include <stdio.h>
#ifndef _In_
#define _In_
#endif
#ifndef _Out_
#define _Out_
#endif
#include "../v3d_engine.h"

static int g_pass, g_fail;
static void check(const char *w, int c)
{ if (c) { g_pass++; printf("  [PASS] %s\n", w); } else { g_fail++; printf("  [FAIL] %s\n", w); } }

int main(void)
{
    V3D_SUBMIT s;
    printf("== V3D submit + MMU engine simulation ==\n");

    /* full job: binner 0x1000..0x1800, render 0x2000..0x2400 */
    V3dSubmitFromCl(0x1000, 0x1800, 0x2000, 0x2400, &s);
    check("CT0CA=bcl_start", s.Ct0Ca == 0x1000);
    check("CT0EA=bcl_end",   s.Ct0Ea == 0x1800);
    check("CT1CA=rcl_start", s.Ct1Ca == 0x2000);
    check("CT1EA=rcl_end",   s.Ct1Ea == 0x2400);
    check("HasBinner",       s.HasBinner == 1);
    check("HasRender",       s.HasRender == 1);

    /* RCL-only blit: empty binner */
    V3dSubmitFromCl(0x1000, 0x1000, 0x2000, 0x2400, &s);
    check("empty BCL -> HasBinner 0", s.HasBinner == 0);
    check("RCL-only still HasRender", s.HasRender == 1);

    /* MMU_CTL value must match the real Pi5 readback (0x060D0C01). */
    check("MMU_CTL == 0x060D0C01", V3dMmuCtlValue() == 0x060D0C01u);

    /* PT base = phys >> 12 */
    check("PT base 0x40000000>>12 == 0x40000", V3dMmuPtBase(0x40000000ull) == 0x40000u);
    check("PT base low bits dropped", V3dMmuPtBase(0x12345abcull) == 0x12345u);

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
