/*++
Module Name: disp_sim.c
Abstract:    x64 simulation of the DOD pure logic: the VideoCore mailbox
             property-message builder (vc_mailbox.c) and the EDID parser (edid.c).
             Build: cl /DDISP_SIM /I.. disp_sim.c ..\vc_mailbox.c ..\edid.c
--*/
#include <stdio.h>
#include "../vc_mailbox.h"
#include "../edid.h"
#include "../hvs_dlist.h"

static int g_pass, g_fail;
static void check(const char *what, int cond)
{
    if (cond) { g_pass++; printf("  [PASS] %s\n", what); }
    else      { g_fail++; printf("  [FAIL] %s\n", what); }
}

static void test_mailbox(void)
{
    ULONG buf[64];
    ULONG n;

    printf("-- VideoCore mailbox property builder --\n");

    /* Set physical width/height (0x00048003) = 1920x1080 */
    n = VcMboxSetPhysSize(buf, 64, 1920, 1080);
    check("set-phys-size total == 32 bytes", n == 32);
    check("buf[0] == total size", buf[0] == 32);
    check("buf[1] == request code 0", buf[1] == 0);
    check("buf[2] == tag SET_PHYS_WH", buf[2] == VCTAG_SET_PHYS_WH);
    check("buf[3] == value buf size 8", buf[3] == 8);
    check("buf[4] == req/resp 0", buf[4] == 0);
    check("buf[5]==1920 buf[6]==1080", buf[5] == 1920 && buf[6] == 1080);
    check("buf[7] == end tag 0", buf[7] == 0);

    /* Get EDID block 0 (0x00030020): value buffer 136 bytes -> 34 words */
    n = VcMboxGetEdidBlock(buf, 64, 0);
    check("get-edid total == 160 bytes", n == (2u + 3u + 34u + 1u) * 4u);
    check("edid tag id", buf[2] == VCTAG_GET_EDID_BLOCK);
    check("edid value buf size 136", buf[3] == 136);
    check("edid block# 0 in value", buf[5] == 0);

    /* capacity overflow -> 0 */
    n = VcMboxSetPhysSize(buf, 6, 1920, 1080);  /* too small for the value words */
    check("overflow returns 0", n == 0);
}

static void build_1080p_edid(UCHAR e[128])
{
    int i, s = 0;
    static const UCHAR hdr[8] = { 0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00 };
    for (i = 0; i < 128; i++) e[i] = 0;
    for (i = 0; i < 8; i++) e[i] = hdr[i];
    /* first DTD @54: pixclk 14850 (148.5MHz), h_active 1920, v_active 1080 */
    e[54] = 0x02; e[55] = 0x3A;          /* pixel clock /10kHz = 0x3A02 = 14850 */
    e[56] = 0x80;                         /* h active low 8 = 0x80 */
    e[58] = 0x70;                         /* high nibble = h active [11:8] = 7 */
    e[59] = 0x38;                         /* v active low 8 = 0x38 */
    e[61] = 0x40;                         /* high nibble = v active [11:8] = 4 */
    /* checksum byte makes the 128-byte sum == 0 (mod 256) */
    for (i = 0; i < 127; i++) s += e[i];
    e[127] = (UCHAR)((256 - (s & 0xFF)) & 0xFF);
}

static void test_edid(void)
{
    UCHAR e[128];
    EDID_INFO info;
    int rc;

    printf("-- EDID parser --\n");
    build_1080p_edid(e);

    rc = EdidParse(e, 128, &info);
    check("parse ok", rc == 0 && info.Valid);
    check("width 1920",  info.Width == 1920);
    check("height 1080", info.Height == 1080);
    check("pixel clock 148500 kHz", info.PixelClockKHz == 148500);

    /* B3: the built-in fallback EDID must round-trip through the parser */
    {
        UCHAR fb[128];
        EDID_INFO fi;
        EdidGetDefault1080p(fb);
        check("default 1080p EDID parses", EdidParse(fb, 128, &fi) == 0);
        check("default is 1920x1080", fi.Width == 1920 && fi.Height == 1080);
        check("default pixclk 148500", fi.PixelClockKHz == 148500);
    }

    /* bad header */
    e[0] = 0x11;
    check("bad header -> error", EdidParse(e, 128, &info) != 0);
    build_1080p_edid(e);
    /* corrupt checksum */
    e[127] ^= 0xFF;
    check("bad checksum -> error", EdidParse(e, 128, &info) != 0);
    /* short buffer */
    build_1080p_edid(e);
    check("short buffer -> error", EdidParse(e, 64, &info) != 0);
}

static void test_hvs(void)
{
    ULONG ctl0, pos2, dl[8], n;

    printf("-- HVS display-list builder --\n");

    ctl0 = HvsBuildCtl0(HVS_PIXEL_FORMAT_RGBA8888, HVS_PIXEL_ORDER_XRGB,
                        HVS_TILING_LINEAR, 5, 1, 1);
    check("CTL0 pixel format == RGBA8888(7)", (ctl0 & 0xF) == 7);
    check("CTL0 order == XRGB(2)", ((ctl0 >> 13) & 0x3) == 2);
    check("CTL0 tiling == LINEAR(0)", ((ctl0 >> 20) & 0x3) == 0);
    check("CTL0 size words == 5", ((ctl0 >> 24) & 0x3F) == 5);
    check("CTL0 VALID set", (ctl0 & (1u<<30)) != 0);
    check("CTL0 END set", (ctl0 & (1u<<31)) != 0);

    pos2 = HvsBuildPos2(1920, 1080);
    check("POS2 width == 1920", (pos2 & 0xFFF) == 1920);
    check("POS2 height == 1080", ((pos2 >> 16) & 0xFFF) == 1080);

    n = HvsBuildPlaneDlist(dl, 8, 1920, 1080, 7680, 0x40000000u);
    check("plane dlist == 5 words", n == 5);
    check("dlist[2] == POS2(1920x1080)", dl[2] == pos2);
    check("dlist[3] == fb phys 0x40000000", dl[3] == 0x40000000u);
    check("dlist[4] == pitch 7680", dl[4] == 7680);

    n = HvsBuildPlaneDlist(dl, 3, 1920, 1080, 7680, 0x40000000u);
    check("dlist overflow -> 0", n == 0);
}

int main(void)
{
    printf("== RP1 vc4 DOD logic simulation ==\n");
    test_mailbox();
    test_edid();
    test_hvs();
    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
