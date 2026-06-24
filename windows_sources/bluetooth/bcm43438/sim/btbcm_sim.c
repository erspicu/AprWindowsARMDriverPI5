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

static void test_vendor(void)
{
    UCHAR p6[6];
    /* Pi5 DT: max-speed 3,000,000 ; local-bd-address d6 5f 58 9e a2 88 */
    static const UCHAR mac[6] = { 0x88, 0xA2, 0x9E, 0x58, 0x5F, 0xD6 };

    printf("-- BCM vendor payloads (Pi5 real values) --\n");

    BtBcmBuildBaudRatePayload(3000000, p6);
    check("baud 3M -> 00 00 c0 c6 2d 00",
          p6[0]==0x00 && p6[1]==0x00 && p6[2]==0xC0 && p6[3]==0xC6 && p6[4]==0x2D && p6[5]==0x00);

    BtBcmBuildBdAddrPayload(mac, p6);
    check("BD_ADDR reversed -> d6 5f 58 9e a2 88",
          p6[0]==0xD6 && p6[1]==0x5F && p6[2]==0x58 && p6[3]==0x9E && p6[4]==0xA2 && p6[5]==0x88);

    /* .hcd record parser: synthetic stream of two records [opcode LE][len][data] */
    {
        /* rec1: Write_RAM 0xFC4C len2 {AA BB}; rec2: Launch_RAM 0xFC4E len4 {FF*4} */
        static const UCHAR hcd[] = {
            0x4C, 0xFC, 0x02, 0xAA, 0xBB,
            0x4E, 0xFC, 0x04, 0xFF, 0xFF, 0xFF, 0xFF
        };
        UCHAR out[64];
        ULONG off = 0, consumed = 0, frame, n = 0;

        frame = BtBcmHcdNextCommand(hcd, sizeof(hcd), off, out, sizeof(out), &consumed);
        check("hcd rec1 framed (01 4c fc 02 aa bb)",
              frame==6 && out[0]==0x01 && out[1]==0x4C && out[2]==0xFC && out[3]==0x02 &&
              out[4]==0xAA && out[5]==0xBB);
        check("hcd rec1 consumed 5", consumed==5);
        off += consumed; n++;

        frame = BtBcmHcdNextCommand(hcd, sizeof(hcd), off, out, sizeof(out), &consumed);
        check("hcd rec2 framed (01 4e fc 04 ff..)",
              frame==8 && out[1]==0x4E && out[2]==0xFC && out[3]==0x04 && out[7]==0xFF);
        check("hcd rec2 consumed 7", consumed==7);
        off += consumed; n++;

        frame = BtBcmHcdNextCommand(hcd, sizeof(hcd), off, out, sizeof(out), &consumed);
        check("hcd end -> 0", frame==0 && consumed==0);
        check("parsed 2 records, consumed all", n==2 && off==sizeof(hcd));

        /* truncated record (len says 4 but only 1 data byte present) */
        {
            static const UCHAR bad[] = { 0x4C, 0xFC, 0x04, 0x11 };
            frame = BtBcmHcdNextCommand(bad, sizeof(bad), 0, out, sizeof(out), &consumed);
            check("truncated hcd record -> 0", frame==0);
        }
    }
}

/* mock UART Tx: record the opcode of each command the state machine sends */
static USHORT g_txlog[64];
static int    g_txn;
static int mock_tx(PVOID ctx, const UCHAR *d, ULONG n)
{
    (void)ctx; (void)n;
    if (g_txn < 64) g_txlog[g_txn++] = (USHORT)d[1] | ((USHORT)d[2] << 8);
    return 0;
}

static void test_init(void)
{
    /* synthetic .hcd: two Write_RAM (0xFC4C) records */
    static const UCHAR hcd[] = { 0x4C,0xFC,0x02,0xAA,0xBB,  0x4C,0xFC,0x02,0xCC,0xDD };
    static const UCHAR mac[6] = { 0x88,0xA2,0x9E,0x58,0x5F,0xD6 };
    static const USHORT exp[] = { 0x0C03,0xFC2E,0xFC4C,0xFC4C,0xFC4E,0x0C03,0xFC01,0xFC18 };
    BTI s;
    int guard = 0, ok = 1, i;

    printf("-- BCM bring-up init state machine --\n");

    g_txn = 0;
    BtBcmInitStart(&s, 3000000, mac, hcd, sizeof(hcd), mock_tx, (PVOID)0);
    /* mock chip answers each pending opcode with a success Command Complete */
    while (!s.Done && !s.Error && guard++ < 1000) {
        UCHAR ev[7];
        ev[0]=0x04; ev[1]=0x0E; ev[2]=0x04; ev[3]=0x01;
        ev[4]=(UCHAR)(s.Pending & 0xFF); ev[5]=(UCHAR)((s.Pending>>8)&0xFF); ev[6]=0x00;
        BtBcmInitOnEvent(&s, ev, 7);
    }
    check("init reached DONE (no error)", s.Done && !s.Error);
    check("8 commands sent", g_txn == 8);
    for (i = 0; i < 8 && i < g_txn; i++) { if (g_txlog[i] != exp[i]) ok = 0; }
    check("opcode order = Reset,Download,2xWriteRAM,Launch,Reset,SetBDADDR,SetBaud",
          ok && g_txn == 8);

    /* a non-zero Command Complete status must fault the machine */
    {
        BTI s2; UCHAR ev[7];
        g_txn = 0;
        BtBcmInitStart(&s2, 3000000, mac, hcd, sizeof(hcd), mock_tx, (PVOID)0);
        ev[0]=0x04; ev[1]=0x0E; ev[2]=0x04; ev[3]=0x01;
        ev[4]=(UCHAR)(s2.Pending & 0xFF); ev[5]=(UCHAR)((s2.Pending>>8)&0xFF); ev[6]=0x01;
        BtBcmInitOnEvent(&s2, ev, 7);
        check("non-zero status -> Error (no DONE)", s2.Error && !s2.Done);
    }
}

int main(void)
{
    printf("== BCM43438 BT HCI simulation ==\n");
    test_framing();
    test_h4_rx();
    test_vendor();
    test_init();
    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
