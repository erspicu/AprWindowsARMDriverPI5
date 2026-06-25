/*++ x64 sim of the SAND(COL128)->linear detile. Build: cl /DHEVC_SIM /I.. hevc_sim.c ..\sand.c --*/
#include <stdio.h>
#include "../sand.h"

static int g_pass, g_fail;
static void check(const char *w, int c)
{ if (c) { g_pass++; printf("  [PASS] %s\n", w); } else { g_fail++; printf("  [FAIL] %s\n", w); } }

#define W 200u   /* spans 2 columns (0..127, 128..199) */
#define H 4u

int main(void)
{
    static UCHAR sand[2 * H * 128];   /* 2 cols * colHeight(H) * 128 */
    static UCHAR lin[W * H];
    ULONG x, y;
    int ok = 1;

    printf("== rpivid SAND(COL128) detile simulation ==\n");

    check("col boundary x=127 -> col0 end", SandColOffset(127, 0, H) == 127);
    check("x=128 -> col1 start", SandColOffset(128, 0, H) == (H * 128u));
    check("(x,y) within col uses y*128", SandColOffset(5, 2, H) == 2 * 128u + 5);

    /* fill SAND so each luma pixel = (x*7+y*13)&0xFF, then detile + verify */
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++)
            sand[SandColOffset(x, y, H)] = (UCHAR)((x * 7u + y * 13u) & 0xFF);

    SandToLinearY(sand, lin, W, H, H);

    for (y = 0; y < H && ok; y++)
        for (x = 0; x < W; x++)
            if (lin[y * W + x] != (UCHAR)((x * 7u + y * 13u) & 0xFF)) { ok = 0; break; }
    check("detiled linear matches source pattern", ok);
    check("spot lin[0,0]", lin[0] == 0);
    check("spot lin[150,3] (col1)", lin[3 * W + 150] == (UCHAR)((150 * 7u + 3 * 13u) & 0xFF));

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
