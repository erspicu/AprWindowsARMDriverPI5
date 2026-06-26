/*++
Module Name: v3d_pte.c
Abstract:    V3D MMU PTE encoder. See v3d_pte.h. Ported from Linux v3d_mmu.c.
--*/
#include "v3d_pte.h"

/* Both the BO page index and the page-address (pfn) must be aligned to the
   large-page size in pages (v3d_mmu_is_aligned). */
static int V3dAligned(U32 page, U32 pfn, U32 alignPages)
{
    return ((page & (alignPages - 1)) == 0) && ((pfn & (alignPages - 1)) == 0);
}

U32 V3dPteEncode(U64 Phys, int Writeable, U32 Page, U32 RemainingBytes)
{
    U32 pfn = (U32)(Phys >> V3D_MMU_PAGE_SHIFT);
    U32 pte = pfn | V3D_PTE_VALID;

    if (Writeable) {
        pte |= V3D_PTE_WRITEABLE;
    }
    /* 1MB superpage = 256 pages; 64KB bigpage = 16 pages. */
    if (RemainingBytes >= (1u << 20) && V3dAligned(Page, pfn, 256)) {
        pte |= V3D_PTE_SUPERPAGE;
    } else if (RemainingBytes >= (1u << 16) && V3dAligned(Page, pfn, 16)) {
        pte |= V3D_PTE_BIGPAGE;
    }
    return pte;
}
