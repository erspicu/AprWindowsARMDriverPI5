/*++ x64 sim of RPi OTP mailbox build/parse. Build: cl /DOTP_SIM /I.. otp_sim.c ..\otp.c --*/
#include <stdio.h>
#include "../otp.h"

static int g_pass, g_fail;
static void check(const char *w, int c)
{ if (c) { g_pass++; printf("  [PASS] %s\n", w); } else { g_fail++; printf("  [FAIL] %s\n", w); } }

int main(void)
{
    ULONG buf[32];
    ULONG rows[4];
    ULONG n, total;

    printf("== RPi OTP mailbox simulation ==\n");

    /* read 2 words of USER OTP from block 0, start row 0 */
    total = OtpBuildReadMsg(buf, 32, RPI_TAG_GET_USER_OTP, 0, 0, 2);
    /* words: 2 hdr + 3 tag-hdr + 5 value(block,row,words,d0,d1) + 1 end = 11 -> 44 bytes */
    check("total == 44 bytes", total == 44);
    check("buf[0] == total", buf[0] == 44);
    check("buf[1] == request 0", buf[1] == 0);
    check("tag == GET_USER_OTP", buf[2] == RPI_TAG_GET_USER_OTP);
    check("value buf size == 20", buf[3] == 20);          /* (3+2)*4 */
    check("block 0 / startRow 0 / words 2", buf[5] == 0 && buf[6] == 0 && buf[7] == 2);
    check("end tag 0", buf[10] == 0);

    /* simulate the firmware filling the response data, then parse */
    buf[8] = 0xDEADBEEF; buf[9] = 0x12345678;
    n = OtpParseResponse(buf, rows, 4);
    check("parsed 2 rows", n == 2);
    check("row0 == 0xDEADBEEF", rows[0] == 0xDEADBEEF);
    check("row1 == 0x12345678", rows[1] == 0x12345678);

    /* customer OTP tag + overflow guard */
    total = OtpBuildReadMsg(buf, 32, RPI_TAG_GET_CUSTOMER_OTP, 1, 5, 3);
    check("customer tag + block1 row5 words3", buf[2] == RPI_TAG_GET_CUSTOMER_OTP && buf[5]==1 && buf[6]==5 && buf[7]==3);
    check("overflow -> 0", OtpBuildReadMsg(buf, 6, RPI_TAG_GET_USER_OTP, 0, 0, 4) == 0);

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
