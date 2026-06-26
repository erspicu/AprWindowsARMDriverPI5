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

/* V3D 7.1 register offsets. NOTE: hub and core are SEPARATE MMIO blocks (Pi5 DT
   v3d@2000000: hub @0x1002000000, core0 @0x1002008000, sms @0x1002030800).
   HUB regs are hub-relative; CTL/CLE/MMU regs are core-relative.
   Real values from read-only Pi5 probe (see 20260626-0330 note). */
#define V3D_HUB_IDENT0      0x0008   /* hub: 0x42554856 "VHUB"                    */
#define V3D_CTL_IDENT0      0x0000   /* core: 0x07443356, VER = bits[31:24] = 7   */
#define V3D_CTL_IDENT1      0x0004   /* core: 0x81001431                         */
#define V3D_MMU_CTL         0x1200   /* core MMU control (page table / TLB flush) */
#define V3D_MMU_CTL_TLB_CLEAR  (1u << 2)  /* v3d_regs.h: TLB_CLEAR = BIT(2)        */
#define V3D_CLE_CT0CA       0x0110   /* binner control-list current addr         */
#define V3D_CLE_CT0EA       0x0108   /* binner control-list end addr (EA>CA=go)  */
#define V3D_CLE_CT1CA       0x0114   /* render control-list current addr         */
#define V3D_CLE_CT1EA       0x010c   /* render control-list end addr             */

typedef struct _RP1V3D_ADAPTER {
    PVOID             DxgkHandle;       /* MiniportDeviceContext == this          */
    DXGKRNL_INTERFACE DxgkInterface;    /* dxgkrnl callbacks (valid after Start)   */
    /* Three V3D MMIO blocks, in ACPI _CRS order: hub / core0 / sms.              */
    PUCHAR            HubRegs;          /* @0x1002000000, 0x4000                   */
    SIZE_T            HubLen;
    PUCHAR            CoreRegs;         /* @0x1002008000, 0x6000 (CTL/CLE/MMU)     */
    SIZE_T            CoreLen;
    PUCHAR            SmsRegs;          /* @0x1002030800, 0x700                    */
    SIZE_T            SmsLen;
    ULONG             CoreIdent0;       /* read at StartDevice to prove hardware  */
    ULONG             HubIdent0;
} RP1V3D_ADAPTER, *PRP1V3D_ADAPTER;

/* MMIO accessors (pass the block base: a->CoreRegs or a->HubRegs). */
__forceinline ULONG V3dRd(_In_ PUCHAR base, _In_ ULONG off)
{ return READ_REGISTER_ULONG((volatile ULONG *)(base + off)); }
__forceinline VOID  V3dWr(_In_ PUCHAR base, _In_ ULONG off, _In_ ULONG v)
{ WRITE_REGISTER_ULONG((volatile ULONG *)(base + off), v); }
