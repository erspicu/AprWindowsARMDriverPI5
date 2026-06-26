/*++
Module Name: common.h
Abstract:    RP1/V3D (VideoCore VII) WDDM full render KMD - scaffold.
             Unlike the DOD, a render KMD registers via DxgkInitialize with
             DRIVER_INITIALIZATION_DATA and must implement the render DDIs
             (CreateDevice/CreateAllocation/Patch/SubmitCommand/Render/...).
             A functional render driver also needs a user-mode D3D driver (UMD)
             and the physical V3D - so this is a buildable scaffold only.
--*/
#pragma once

#include <ntddk.h>
#include <dispmprt.h>

#define RP1V3D_POOLTAG  '3DV1'   // 'V3D1'

/* V3D 7.1 register offsets within the V3D MMIO block (BCM2712 @ 0x1002000000;
   see MD/Note/20260625-0200-pi5-display-facts.md). */
#define V3D_CTL_IDENT0      0x0000   /* core version: VER = bits [31:24]        */
#define V3D_HUB_IDENT0      0x0008   /* hub version / nslc                       */
#define V3D_MMU_CTL         0x1200   /* MMU control (page table base / TLB flush)*/
#define V3D_CLE_CT0CA       0x0110   /* binner control-list current addr         */
#define V3D_CLE_CT0EA       0x0108   /* binner control-list end addr (EA>CA=go)  */
#define V3D_CLE_CT1CA       0x0114   /* render control-list current addr         */
#define V3D_CLE_CT1EA       0x010c   /* render control-list end addr             */

typedef struct _RP1V3D_ADAPTER {
    PVOID             DxgkHandle;       /* MiniportDeviceContext == this          */
    DXGKRNL_INTERFACE DxgkInterface;    /* dxgkrnl callbacks (valid after Start)   */
    PHYSICAL_ADDRESS  RegsPhys;         /* V3D MMIO physical base                  */
    PUCHAR            Regs;             /* mapped V3D MMIO                         */
    SIZE_T            RegsLen;
    ULONG             CoreIdent0;       /* read at StartDevice to prove hardware  */
    ULONG             HubIdent0;
} RP1V3D_ADAPTER, *PRP1V3D_ADAPTER;

/* MMIO accessors */
__forceinline ULONG V3dRd(_In_ PRP1V3D_ADAPTER a, _In_ ULONG off)
{ return READ_REGISTER_ULONG((volatile ULONG *)(a->Regs + off)); }
__forceinline VOID  V3dWr(_In_ PRP1V3D_ADAPTER a, _In_ ULONG off, _In_ ULONG v)
{ WRITE_REGISTER_ULONG((volatile ULONG *)(a->Regs + off), v); }
