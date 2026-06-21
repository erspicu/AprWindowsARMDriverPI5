/*++
Module Name: btbcm_sim.c
Abstract:    x64 simulation of the BCM43438 BT HCI framing (hci.c). Verifies the
             H4 HCI command framing + the BCM bring-up step table, without HW.
             Build: cl /DBTBCM_SIM /I.. btbcm_sim.c ..\hci.c
--*/
#include <stdio.h>
#include "../common.h"

static int g_pass, g_fail;
static void check(const char *what, int cond)
{
    if (cond) { g_pass++; printf("  [PASS] %s\n", what); }
    else      { g_fail++; printf("  [FAIL] %s\n", what); }
}

int main(void)
{
    unsigned char buf[32];
    unsigned char small[2];
    unsigned char baud[6] = { 0x00, 0x00, 0x10, 0x0E, 0x00, 0x00 }; /* 921600 LE */
    ULONG n;

    printf("== BCM43438 BT H4 HCI framing simulation ==\n");

    /* HCI_Reset (0x0C03), no parameters */
    n = BtBcmBuildCommand(buf, sizeof(buf), HCI_OP_RESET, NULL, 0);
    check("HCI_Reset frame length == 4", n == 4);
    check("byte0 == H4_CMD", buf[0] == H4_CMD);
    check("opcode LE low == 0x03", buf[1] == 0x03);
    check("opcode LE high == 0x0C", buf[2] == 0x0C);
    check("param len == 0", buf[3] == 0);

    /* BCM set-baud-rate (0xFC18) with 6 param bytes */
    n = BtBcmBuildCommand(buf, sizeof(buf), BCM_OP_SET_BAUD_RATE, baud, 6);
    check("set-baud frame length == 10", n == 10);
    check("byte0 == H4_CMD", buf[0] == H4_CMD);
    check("opcode 0xFC18 LE", buf[1] == 0x18 && buf[2] == 0xFC);
    check("param len == 6", buf[3] == 6);
    check("params copied", buf[4]==0x00 && buf[5]==0x00 && buf[6]==0x10 && buf[7]==0x0E);

    /* buffer-too-small guard */
    n = BtBcmBuildCommand(small, sizeof(small), HCI_OP_RESET, NULL, 0);
    check("too-small buffer returns 0", n == 0);

    /* BCM bring-up step table */
    check("BCM init has 7 steps", BtBcmInitStepCount() == 7);

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
