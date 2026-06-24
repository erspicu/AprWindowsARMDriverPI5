/*++
Module Name: btbcm_sim.c
Abstract:    x64 simulation of the BCM43438 BT HCI logic. Verifies (1) the H4 HCI
             command framing + BCM bring-up step table (hci.c) and (2) the H4 RX
             reassembly state machine (h4_parser.c), all without hardware.
             Build: cl /DBTBCM_SIM /I.. btbcm_sim.c ..\hci.c ..\h4_parser.c
--*/
#include <stdio.h>
#include "../common.h"

static int g_pass, g_fail;
static void check(const char *what, int cond)
{
    if (cond) { g_pass++; printf("  [PASS] %s\n", what); }
    else      { g_fail++; printf("  [FAIL] %s\n", what); }
}

/* capture sink for the H4 RX state machine: record each emitted packet */
#define CAP_MAX 8
typedef struct { UCHAR pkt[H4_RX_MAX_PACKET]; ULONG len; } CAP_PKT;
typedef struct { CAP_PKT p[CAP_MAX]; ULONG n; } CAP;

static void cap_cb(PVOID ctx, const UCHAR *pkt, ULONG len)
{
    CAP *c = (CAP *)ctx;
    if (c->n < CAP_MAX) {
        memcpy(c->p[c->n].pkt, pkt, len);
        c->p[c->n].len = len;
    }
    c->n++;
}

static void test_framing(void)
{
    unsigned char buf[32];
    unsigned char small[2];
    unsigned char baud[6] = { 0x00, 0x00, 0x10, 0x0E, 0x00, 0x00 }; /* 921600 LE */
    ULONG n;

    printf("-- H4 command framing --\n");

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
    check("opcode 0xFC18 LE", buf[1] == 0x18 && buf[2] == 0xFC);
    check("param len == 6", buf[3] == 6);
    check("params copied", buf[4]==0x00 && buf[5]==0x00 && buf[6]==0x10 && buf[7]==0x0E);

    /* buffer-too-small guard */
    n = BtBcmBuildCommand(small, sizeof(small), HCI_OP_RESET, NULL, 0);
    check("too-small buffer returns 0", n == 0);

    check("BCM init has 7 steps", BtBcmInitStepCount() == 7);
}

static void test_h4_rx(void)
{
    /* Command Complete event for HCI_Reset: 04 0E 04 01 03 0C 00 */
    static const UCHAR cc_reset[] = { 0x04, 0x0E, 0x04, 0x01, 0x03, 0x0C, 0x00 };
    /* ACL packet: type 02, handle 0x0001, datalen 0x0003 (LE) + 3 payload bytes */
    static const UCHAR acl[] = { 0x02, 0x01, 0x00, 0x03, 0x00, 0xAA, 0xBB, 0xCC };
    CAP cap;
    H4_RX rx;
    ULONG i, em;
    UCHAR status = 0xFF;

    printf("-- H4 RX reassembly state machine --\n");

    /* (1) one whole event in a single feed */
    cap.n = 0; H4RxInit(&rx, cap_cb, &cap);
    em = H4RxFeed(&rx, cc_reset, sizeof(cc_reset));
    check("whole event -> 1 packet", em == 1 && cap.n == 1);
    check("event len == 7", cap.n >= 1 && cap.p[0].len == 7);
    check("event type 0x04", cap.n >= 1 && cap.p[0].pkt[0] == 0x04);
    check("is CmdComplete(0x0C03), status 0",
          H4IsCommandComplete(cap.p[0].pkt, cap.p[0].len, HCI_OP_RESET, &status) && status == 0);

    /* (2) same event fed one byte at a time (worst-case UART fragmentation) */
    cap.n = 0; H4RxInit(&rx, cap_cb, &cap); em = 0;
    for (i = 0; i < sizeof(cc_reset); i++) {
        em += H4RxFeed(&rx, &cc_reset[i], 1);
    }
    check("byte-by-byte -> 1 packet", em == 1 && cap.n == 1 && cap.p[0].len == 7);

    /* (3) two packets concatenated in one feed (event + ACL) */
    {
        UCHAR both[sizeof(cc_reset) + sizeof(acl)];
        memcpy(both, cc_reset, sizeof(cc_reset));
        memcpy(both + sizeof(cc_reset), acl, sizeof(acl));
        cap.n = 0; H4RxInit(&rx, cap_cb, &cap);
        em = H4RxFeed(&rx, both, sizeof(both));
        check("concatenated -> 2 packets", em == 2 && cap.n == 2);
        check("pkt2 is ACL len 8", cap.n == 2 && cap.p[1].pkt[0] == 0x02 && cap.p[1].len == 8);
    }

    /* (4) ACL 2-byte LE length decoded correctly across a split */
    cap.n = 0; H4RxInit(&rx, cap_cb, &cap);
    H4RxFeed(&rx, acl, 4);                 /* type + header only */
    check("after header, no packet yet", cap.n == 0);
    em = H4RxFeed(&rx, acl + 4, 4);        /* the 3 payload bytes (+ nothing extra) */
    check("ACL completes -> 1 packet len 8", em == 1 && cap.n == 1 && cap.p[0].len == 8);
    check("ACL payload intact", cap.n == 1 && cap.p[0].pkt[5]==0xAA && cap.p[0].pkt[7]==0xCC);

    /* (5) unknown leading byte is dropped, machine resyncs to next valid packet */
    {
        UCHAR junk[] = { 0xFF, 0x99 };
        cap.n = 0; H4RxInit(&rx, cap_cb, &cap);
        H4RxFeed(&rx, junk, sizeof(junk));
        em = H4RxFeed(&rx, cc_reset, sizeof(cc_reset));
        check("resync after junk -> 1 packet", em == 1 && cap.n == 1 && cap.p[0].len == 7);
    }
}

int main(void)
{
    printf("== BCM43438 BT HCI simulation ==\n");
    test_framing();
    test_h4_rx();
    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
