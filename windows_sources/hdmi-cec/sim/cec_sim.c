/*++ x64 sim of HDMI-CEC build/parse. Build: cl /DCEC_SIM /I.. cec_sim.c ..\cec.c --*/
#include <stdio.h>
#include "../cec.h"

static int g_pass, g_fail;
static void check(const char *w, int c)
{ if (c) { g_pass++; printf("  [PASS] %s\n", w); } else { g_fail++; printf("  [FAIL] %s\n", w); } }

int main(void)
{
    UCHAR msg[CEC_MAX_MSG_SIZE];
    UCHAR pa[2] = { 0x10, 0x00 };   /* physical address 1.0.0.0 */
    ULONG n;

    printf("== HDMI-CEC message simulation ==\n");

    /* <Active Source> from logical addr 4 (Playback 1), broadcast (0xF) */
    n = CecBuildMsg(msg, sizeof(msg), 4, CEC_LOG_ADDR_BROADCAST, CEC_MSG_ACTIVE_SOURCE, pa, 2);
    check("len == 4 (hdr+op+2 operands)", n == 4);
    check("header == 0x4F (init4 dest F)", msg[0] == 0x4F);
    check("opcode == ACTIVE_SOURCE", msg[1] == CEC_MSG_ACTIVE_SOURCE);
    check("operand phys addr copied", msg[2] == 0x10 && msg[3] == 0x00);
    check("initiator == 4", CecMsgInitiator(msg) == 4);
    check("destination == 0xF", CecMsgDestination(msg) == 0xF);
    check("is broadcast", CecMsgIsBroadcast(msg) == 1);

    /* <Image View On> from 4 to TV (0), no operands */
    n = CecBuildMsg(msg, sizeof(msg), 4, 0, CEC_MSG_IMAGE_VIEW_ON, 0, 0);
    check("len == 2 (hdr+op)", n == 2);
    check("header == 0x40", msg[0] == 0x40);
    check("opcode == IMAGE_VIEW_ON", msg[1] == CEC_MSG_IMAGE_VIEW_ON);
    check("not broadcast (dest 0)", CecMsgIsBroadcast(msg) == 0);

    /* guards */
    check("bad logical addr -> 0", CecBuildMsg(msg, sizeof(msg), 0x10, 0, 0x04, 0, 0) == 0);
    check("overflow -> 0", CecBuildMsg(msg, sizeof(msg), 4, 0, 0x82, pa, 20) == 0);

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
