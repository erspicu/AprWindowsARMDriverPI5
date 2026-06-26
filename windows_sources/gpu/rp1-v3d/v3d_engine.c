/*++
Module Name: v3d_engine.c
Abstract:    V3D submit + MMU engine logic. See v3d_engine.h.
--*/
#include "v3d_engine.h"

/* V3D_MMU_CTL bits (v3d_regs.h). */
#define V3D_MMU_CTL_ENABLE                  (1u << 0)
#define V3D_MMU_CTL_WRITE_VIOLATION_INT     (1u << 10)
#define V3D_MMU_CTL_WRITE_VIOLATION_ABORT   (1u << 11)
#define V3D_MMU_CTL_PT_INVALID_ENABLE       (1u << 16)
#define V3D_MMU_CTL_PT_INVALID_INT          (1u << 18)
#define V3D_MMU_CTL_PT_INVALID_ABORT        (1u << 19)
#define V3D_MMU_CTL_CAP_EXCEEDED_INT        (1u << 25)
#define V3D_MMU_CTL_CAP_EXCEEDED_ABORT      (1u << 26)

void V3dSubmitFromCl(U32 BclStart, U32 BclEnd, U32 RclStart, U32 RclEnd, V3D_SUBMIT *Out)
{
    /* CT current = start, CT end = end. Writing EA (> CA) starts the thread. */
    Out->Ct0Ca = BclStart;  Out->Ct0Ea = BclEnd;
    Out->Ct1Ca = RclStart;  Out->Ct1Ea = RclEnd;
    Out->HasBinner = (BclEnd > BclStart) ? 1 : 0;
    Out->HasRender = (RclEnd > RclStart) ? 1 : 0;
}

U32 V3dMmuCtlValue(void)
{
    return V3D_MMU_CTL_ENABLE |
           V3D_MMU_CTL_PT_INVALID_ENABLE |
           V3D_MMU_CTL_PT_INVALID_ABORT |
           V3D_MMU_CTL_PT_INVALID_INT |
           V3D_MMU_CTL_WRITE_VIOLATION_ABORT |
           V3D_MMU_CTL_WRITE_VIOLATION_INT |
           V3D_MMU_CTL_CAP_EXCEEDED_ABORT |
           V3D_MMU_CTL_CAP_EXCEEDED_INT;   /* = 0x060D0C01 */
}

U32 V3dMmuPtBase(U64 PtPhys)
{
    return (U32)(PtPhys >> 12);   /* V3D_MMU_PAGE_SHIFT */
}
