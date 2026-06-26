/* x64 sim of the V3D MMU PTE encoder. Build: cl /DV3D_PTE_SIM /I.. pte_sim.c ..\v3d_pte.c */
#include <stdio.h>
#ifndef _In_
#define _In_
#endif
#include "../v3d_pte.h"

static int g_pass, g_fail;
static void check(const char *w, int c)
{ if (c) { g_pass++; printf("  [PASS] %s\n", w); } else { g_fail++; printf("  [FAIL] %s\n", w); } }

int main(void)
{
    printf("== V3D MMU PTE encoder simulation ==\n");

    /* basic 4KB page @ phys 0x12345000, writeable, not large-aligned */
    U32 pte = V3dPteEncode(0x12345000ull, 1, 5, 0x1000);
    check("VALID set",      (pte & V3D_PTE_VALID) != 0);
    check("WRITEABLE set",  (pte & V3D_PTE_WRITEABLE) != 0);
    check("pfn == 0x12345", (pte & 0x000fffff) == 0x12345);
    check("no super/big",   (pte & (V3D_PTE_SUPERPAGE | V3D_PTE_BIGPAGE)) == 0);

    /* read-only page: no WRITEABLE */
    check("read-only clears WRITEABLE", (V3dPteEncode(0x1000ull, 0, 0, 0x1000) & V3D_PTE_WRITEABLE) == 0);

    /* 1MB superpage: phys + page both 256-page aligned, >=1MB remaining.
       pfn = 0x100000>>... use phys 0x40000000 (pfn 0x40000, /256 aligned), page 0 */
    pte = V3dPteEncode(0x40000000ull, 1, 0, 1u << 20);
    check("1MB-aligned -> SUPERPAGE", (pte & V3D_PTE_SUPERPAGE) != 0);

    /* same address but only 4KB remaining -> no superpage */
    check("aligned but <1MB left -> no super",
          (V3dPteEncode(0x40000000ull, 1, 0, 0x1000) & V3D_PTE_SUPERPAGE) == 0);

    /* 64KB bigpage: 16-page aligned (phys 0x40010000 pfn 0x40010, page 16), >=64KB */
    pte = V3dPteEncode(0x40010000ull, 1, 16, 1u << 16);
    check("64KB-aligned -> BIGPAGE",  (pte & V3D_PTE_BIGPAGE) != 0);
    check("BIGPAGE not SUPERPAGE",    (pte & V3D_PTE_SUPERPAGE) == 0);

    /* misaligned page index defeats superpage even if phys aligned */
    check("misaligned page idx -> no super",
          (V3dPteEncode(0x40000000ull, 1, 7, 1u << 20) & V3D_PTE_SUPERPAGE) == 0);

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
