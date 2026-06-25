/*++
Module Name: hvs_dlist.h
Abstract:    BCM2712 HVS (Hardware Video Scaler) display-list builder for the DOD
             "flip" path: build the SCALER control words for a single linear
             XRGB8888 plane pointing at a framebuffer. The HVS scans this dlist
             from SRAM; writing the dlist pointer to SCALER_DISPLISTX(channel)
             makes the plane visible (Phase D).

   Field encodings are taken from the local Linux source
   (sources/drivers/gpu/drm/vc4/vc4_regs.h): SCALER_CTL0 END=bit31, VALID=bit30,
   SIZE=[29:24], TILING=[21:20], ORDER=[14:13], PIXEL_FORMAT=[3:0];
   SCALER_POS2 HEIGHT=[27:16], WIDTH=[11:0]; HVS_PIXEL_FORMAT_RGBA8888=7,
   HVS_PIXEL_ORDER_XRGB=2, TILING_LINEAR=0.

   NOTE: Pi5 is gen5/vc6 — the field *encodings* below are real, but the exact
   gen5 dlist *word sequence* (SCALER5 uses a separate size word + pointer
   handling) must be confirmed against vc4_plane.c's hvs5 path + on hardware.
   OS-independent pure logic; x64-sim verified (DISP_SIM).
--*/
#pragma once

#ifdef DISP_SIM
#include "sim/disp_simshim.h"
#else
#include <ntddk.h>
#endif

/* SCALER_CTL0 fields (vc4_regs.h) */
#define HVS_CTL0_END                 (1u << 31)
#define HVS_CTL0_VALID               (1u << 30)
#define HVS_CTL0_SIZE_SHIFT          24      /* [29:24] dlist length in words */
#define HVS_CTL0_TILING_SHIFT        20      /* [21:20] */
#define HVS_CTL0_ORDER_SHIFT         13      /* [14:13] */
#define HVS_CTL0_PIXEL_FORMAT_SHIFT  0       /* [3:0]   */

#define HVS_TILING_LINEAR            0u
#define HVS_PIXEL_FORMAT_RGBA8888    7u
#define HVS_PIXEL_ORDER_XRGB         2u

/* SCALER_POS2 fields */
#define HVS_POS2_HEIGHT_SHIFT        16      /* [27:16] */
#define HVS_POS2_WIDTH_SHIFT         0       /* [11:0]  */

/* build the SCALER_CTL0 control word for a plane */
ULONG HvsBuildCtl0(_In_ ULONG PixelFormat, _In_ ULONG PixelOrder, _In_ ULONG Tiling,
                   _In_ ULONG SizeWords, _In_ int Valid, _In_ int End);

/* build the SCALER_POS2 (width/height) word */
ULONG HvsBuildPos2(_In_ ULONG Width, _In_ ULONG Height);

/*
 * Assemble a minimal single-plane display list (linear XRGB8888) for the given
 * resolution + framebuffer physical address into Dlist (CapWords). Returns the
 * number of u32 words written, or 0 on overflow. Word sequence (gen4-style
 * representative; confirm gen5 against vc4_plane.c):
 *   [0] CTL0  [1] POS0(=0,0)  [2] POS2(w,h)  [3] pointer(fb low)  [4] pitch
 */
ULONG HvsBuildPlaneDlist(_Out_writes_(CapWords) ULONG *Dlist, _In_ ULONG CapWords,
                         _In_ ULONG Width, _In_ ULONG Height, _In_ ULONG Pitch,
                         _In_ ULONG FbPhysLow);
