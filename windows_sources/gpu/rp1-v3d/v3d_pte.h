/*++
Module Name: v3d_pte.h
Abstract:    V3D MMU page-table entry encoder (ported from Linux v3d_mmu.c
             v3d_mmu_insert_ptes). Pure logic — used by the KMD's
             DxgkDdiBuildPagingBuffer(UPDATE_PAGE_TABLE) to translate OS physical
             pages into V3D PTEs. x64-sim verified (V3D_PTE_SIM).
--*/
#pragma once

#ifdef V3D_PTE_SIM
#include <stdint.h>
typedef uint32_t U32; typedef uint64_t U64;
#else
#include <ntddk.h>
typedef ULONG U32; typedef ULONGLONG U64;
#endif

/* V3D MMU: 4KB pages; PTE = pfn | prot bits (v3d_mmu.c). */
#define V3D_MMU_PAGE_SHIFT  12
#define V3D_PTE_SUPERPAGE   (1u << 31)   /* 1MB-aligned run                     */
#define V3D_PTE_BIGPAGE     (1u << 30)   /* 64KB-aligned run                    */
#define V3D_PTE_WRITEABLE   (1u << 29)
#define V3D_PTE_VALID       (1u << 28)

/* Encode one PTE for a 4KB page at physical address Phys.
   Page/RemainingBytes drive the large-page (super/big) optimisation exactly as
   the Linux driver: a super/big page needs BOTH the BO page index and the pfn
   aligned, and enough bytes left in the mapping. */
U32 V3dPteEncode(_In_ U64 Phys, _In_ int Writeable, _In_ U32 Page, _In_ U32 RemainingBytes);
