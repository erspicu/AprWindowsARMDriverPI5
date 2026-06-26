/*++
Module Name: v3d_engine.h
Abstract:    V3D submit + MMU engine logic (pure, sim-verified). Bridges the UMD
             submit (drm_v3d_submit_cl addresses) to the KMD's CLE register
             writes (CT0/CT1 CA/EA), and builds the V3D MMU configuration.
             Ported from Linux v3d_sched.c (CL submit) + v3d_mmu.c (set_page_table).
             x64-sim verified (V3D_ENG_SIM).
--*/
#pragma once

#ifdef V3D_ENG_SIM
#include <stdint.h>
typedef uint32_t U32; typedef uint64_t U64;
#else
#include <ntddk.h>
typedef ULONG U32; typedef ULONGLONG U64;
#endif

/* ---- CL submit (binner CT0 + render CT1). EA>CA triggers execution. ---- */
typedef struct _V3D_SUBMIT {
    U32 Ct0Ca, Ct0Ea;   /* binner control-list current/end (from bcl_start/end) */
    U32 Ct1Ca, Ct1Ea;   /* render control-list current/end (from rcl_start/end) */
    int HasBinner;       /* bcl_end > bcl_start                                  */
    int HasRender;       /* rcl_end > rcl_start                                  */
} V3D_SUBMIT;

/* Map a drm_v3d_submit_cl's BCL/RCL addresses to the CT registers. */
void V3dSubmitFromCl(_In_ U32 BclStart, _In_ U32 BclEnd,
                     _In_ U32 RclStart, _In_ U32 RclEnd, _Out_ V3D_SUBMIT *Out);

/* ---- MMU configuration (v3d_mmu_set_page_table) ---- */
/* V3D_MMU_CTL value to enable the MMU with fault abort/int (== 0x060D0C01,
   confirmed against real Pi5 v3d_regs). */
U32 V3dMmuCtlValue(void);
/* V3D_MMU_PT_PA_BASE value = page-table physical address >> 12. */
U32 V3dMmuPtBase(_In_ U64 PtPhys);
