/*++
Module Name: hvs_dlist.c
Abstract:    BCM2712 HVS display-list builder. See hvs_dlist.h.
--*/
#include "hvs_dlist.h"

ULONG
HvsBuildCtl0(_In_ ULONG PixelFormat, _In_ ULONG PixelOrder, _In_ ULONG Tiling,
             _In_ ULONG SizeWords, _In_ int Valid, _In_ int End)
{
    ULONG w = 0;
    w |= (PixelFormat & 0xFu)  << HVS_CTL0_PIXEL_FORMAT_SHIFT;
    w |= (PixelOrder  & 0x3u)  << HVS_CTL0_ORDER_SHIFT;
    w |= (Tiling      & 0x3u)  << HVS_CTL0_TILING_SHIFT;
    w |= (SizeWords   & 0x3Fu) << HVS_CTL0_SIZE_SHIFT;   /* [29:24] */
    if (Valid) w |= HVS_CTL0_VALID;
    if (End)   w |= HVS_CTL0_END;
    return w;
}

ULONG
HvsBuildPos2(_In_ ULONG Width, _In_ ULONG Height)
{
    return ((Height & 0xFFFu) << HVS_POS2_HEIGHT_SHIFT) |
           ((Width  & 0xFFFu) << HVS_POS2_WIDTH_SHIFT);
}

ULONG
HvsBuildPlaneDlist(_Out_writes_(CapWords) ULONG *Dlist, _In_ ULONG CapWords,
                   _In_ ULONG Width, _In_ ULONG Height, _In_ ULONG Pitch,
                   _In_ ULONG FbPhysLow)
{
    const ULONG words = 5;

    if (CapWords < words) {
        return 0;
    }
    /* CTL0: linear XRGB8888 plane, VALID, dlist length = words, END (single plane) */
    Dlist[0] = HvsBuildCtl0(HVS_PIXEL_FORMAT_RGBA8888, HVS_PIXEL_ORDER_XRGB,
                            HVS_TILING_LINEAR, words, 1 /*valid*/, 1 /*end*/);
    Dlist[1] = 0;                         /* POS0: x=0, y=0, no alpha */
    Dlist[2] = HvsBuildPos2(Width, Height);
    Dlist[3] = FbPhysLow;                 /* plane 0 pointer (framebuffer base) */
    Dlist[4] = Pitch;                     /* plane 0 pitch (bytes per line) */
    return words;
}
