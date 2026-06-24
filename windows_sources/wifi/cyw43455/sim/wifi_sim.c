/*++
Module Name: wifi_sim.c
Abstract:    x64 simulation of the CYW43455 SDIO control plane (sdio_core.c).
             A mock SDIO bus models the backplane address window + a ChipCommon
             register holding chip-id 0x4345, so the "read Chip ID" bring-up
             logic and the WHD NVRAM preprocessor are verified without hardware.
             Build: cl /DCYW_SIM /I.. wifi_sim.c ..\sdio_core.c
--*/
#include <stdio.h>
#include "../sdio_core.h"

static int g_pass, g_fail;
static void check(const char *what, int cond)
{
    if (cond) { g_pass++; printf("  [PASS] %s\n", what); }
    else      { g_fail++; printf("  [FAIL] %s\n", what); }
}

/* ---- mock CYW43455 SDIO bus ---- */
typedef struct {
    UCHAR  winLow, winMid, winHigh;   /* last window registers written */
    int    cmd52Writes;
    int    cmd53Reads;
    UCHAR  log52[8];                  /* values written to LOW/MID/HIGH in order */
    int    log52n;
    int    failAtCmd52;               /* if >0, the Nth cmd52 returns error */
} MOCK_BUS;

static int mock_cmd52(PVOID ctx, UCHAR func, ULONG addr, PUCHAR data, int write)
{
    MOCK_BUS *b = (MOCK_BUS *)ctx;
    (void)func;
    if (write) {
        b->cmd52Writes++;
        if (b->failAtCmd52 && b->cmd52Writes == b->failAtCmd52) return -1;
        if (addr == SBSDIO_FUNC1_SBADDRLOW)  { b->winLow  = *data; }
        if (addr == SBSDIO_FUNC1_SBADDRMID)  { b->winMid  = *data; }
        if (addr == SBSDIO_FUNC1_SBADDRHIGH) { b->winHigh = *data; }
        if (b->log52n < 8) b->log52[b->log52n++] = *data;
    }
    return 0;
}

static int mock_cmd53(PVOID ctx, UCHAR func, ULONG addr, PUCHAR buf, ULONG len, int write)
{
    MOCK_BUS *b = (MOCK_BUS *)ctx;
    ULONG window, full;
    ULONG val = 0xFFFFFFFFu;
    ULONG i;
    (void)func;
    if (write) return -1;
    b->cmd53Reads++;

    /* reconstruct the full backplane address from window regs + 15-bit offset */
    window = ((ULONG)b->winHigh << 24) | ((ULONG)b->winMid << 16) | ((ULONG)b->winLow << 8);
    full   = window | (addr & SBSDIO_SB_OFT_ADDR_MASK);

    if (full == CYW43455_CHIPCOMMON_BASE) {
        val = 0x00004345u;   /* ChipCommon: low16 = chip id 0x4345 */
    }
    for (i = 0; i < len && i < 4; i++) {
        buf[i] = (UCHAR)((val >> (8 * i)) & 0xFF);
    }
    return 0;
}

static void test_chipid(void)
{
    MOCK_BUS bus;
    SDIO_OPS ops;
    USHORT id = 0;
    int rc;

    printf("-- SDIO backplane chip-id read --\n");
    memset(&bus, 0, sizeof(bus));
    ops.Cmd52 = mock_cmd52; ops.Cmd53 = mock_cmd53; ops.Ctx = &bus;

    rc = Cyw43455ReadChipId(&ops, &id);
    check("ReadChipId returns success", rc == 0);
    check("chip id == 0x4345", id == CYW43455_CHIP_ID);
    check("3 window writes issued", bus.cmd52Writes == 3);
    check("1 backplane read issued", bus.cmd53Reads == 1);
    check("window LOW/MID/HIGH = 00/00/18",
          bus.winLow == 0x00 && bus.winMid == 0x00 && bus.winHigh == 0x18);
    check("window written in LOW,MID,HIGH order",
          bus.log52n == 3 && bus.log52[0]==0x00 && bus.log52[1]==0x00 && bus.log52[2]==0x18);

    /* error propagation: fail the 2nd cmd52 (the MID window write) */
    memset(&bus, 0, sizeof(bus)); bus.failAtCmd52 = 2;
    ops.Ctx = &bus; id = 0;
    rc = Cyw43455ReadChipId(&ops, &id);
    check("cmd52 error propagates (rc != 0)", rc != 0);
    check("no backplane read after window error", bus.cmd53Reads == 0);
}

static void test_nvram(void)
{
    /* comments, a blank line, CRLF line endings, no trailing newline */
    static const char in[] =
        "# Pi nvram for cyw43455\r\n"
        "manfid=0x2d0\r\n"
        "\r\n"
        "  # indented comment\r\n"
        "boardrev=0x1304";
    UCHAR out[128];
    ULONG n, k, nulls;

    printf("-- WHD NVRAM preprocess --\n");
    n = WhdNvramPreprocess(in, (ULONG)(sizeof(in) - 1), out, sizeof(out));
    check("output non-empty", n > 0);

    /* expect exactly: "manfid=0x2d0\0boardrev=0x1304\0\0" */
    {
        /* "manfid=0x2d0"(12) \0  "boardrev=0x1304"(15) \0  \0  = 30 */
        ULONG explen = 12 + 1 + 15 + 1 + 1;
        check("length == 30", n == explen);
        check("starts with manfid=0x2d0", memcmp(out, "manfid=0x2d0", 12) == 0);
        check("first NUL separator at [12]", out[12] == 0x00);
        check("second line present", memcmp(out + 13, "boardrev=0x1304", 15) == 0);
        check("ends with double NUL", n >= 2 && out[n-1] == 0x00 && out[n-2] == 0x00);
    }

    /* no '\n' should survive; comment/blank lines fully dropped */
    nulls = 0;
    for (k = 0; k < n; k++) {
        if (out[k] == '\n' || out[k] == '#') { check("no raw newline/comment byte", 0); break; }
        if (out[k] == 0x00) nulls++;
    }
    check("exactly 3 NUL bytes (2 sep + 1 final)", nulls == 3);

    /* overflow guard */
    n = WhdNvramPreprocess(in, (ULONG)(sizeof(in) - 1), out, 8);
    check("overflow returns 0", n == 0);
}

int main(void)
{
    printf("== CYW43455 SDIO control-plane simulation ==\n");
    test_chipid();
    test_nvram();
    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
