/*++ x64 sim of RAW10 unpack + Bayer classify. Build: cl /DISP_SIM /I.. isp_sim.c ..\isp.c --*/
#include <stdio.h>
#include "../isp.h"

static int g_pass, g_fail;
static void check(const char *w, int c)
{ if (c) { g_pass++; printf("  [PASS] %s\n", w); } else { g_fail++; printf("  [FAIL] %s\n", w); } }

int main(void)
{
    /* px = {0x3FF, 0x000, 0x155, 0x2AA}: b0..b3 = MSBs, b4 = packed LSBs */
    UCHAR packed[5];
    USHORT px[4];
    ULONG n;

    printf("== camera ISP (RAW10 unpack + Bayer) simulation ==\n");

    packed[0] = 0xFF; packed[1] = 0x00; packed[2] = 0x55; packed[3] = 0xAA;
    packed[4] = (UCHAR)((0x3FF & 3) | ((0x000 & 3) << 2) | ((0x155 & 3) << 4) | ((0x2AA & 3) << 6));
    n = Raw10Unpack(packed, px, 1);
    check("unpacked 4 pixels", n == 4);
    check("px0 == 0x3FF", px[0] == 0x3FF);
    check("px1 == 0x000", px[1] == 0x000);
    check("px2 == 0x155", px[2] == 0x155);
    check("px3 == 0x2AA", px[3] == 0x2AA);
    check("all pixels 10-bit", (px[0]|px[1]|px[2]|px[3]) <= 0x3FF);

    printf("-- Bayer RGGB classify --\n");
    check("(0,0) = R", BayerColorRGGB(0,0) == BAYER_R);
    check("(1,0) = G", BayerColorRGGB(1,0) == BAYER_G);
    check("(0,1) = G", BayerColorRGGB(0,1) == BAYER_G);
    check("(1,1) = B", BayerColorRGGB(1,1) == BAYER_B);
    check("(2,2) = R (period 2)", BayerColorRGGB(2,2) == BAYER_R);

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
