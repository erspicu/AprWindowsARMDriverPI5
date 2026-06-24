/*++
Module Name: h4_parser.c
Abstract:    H4 UART HCI receive state machine. Reassembles a raw UART byte
             stream (which has no frame boundaries) into complete HCI packets,
             driven incrementally so it tolerates arbitrary fragmentation
             (the UART delivers whatever-sized chunks). Ported from the framing
             logic in Linux hci_h4.c. Pure logic - x64 sim verifiable (BTBCM_SIM).

   Per-type header layout (bytes AFTER the leading H4 type byte):
     0x01 Command : opcode[2] paramlen[1]   -> payload = hdr[2]            (hdr 3)
     0x02 ACL     : handle[2] datalen[2 LE] -> payload = hdr[2]|hdr[3]<<8  (hdr 4)
     0x03 SCO     : handle[2] datalen[1]    -> payload = hdr[2]            (hdr 3)
     0x04 Event   : code[1]   paramlen[1]   -> payload = hdr[1]            (hdr 2)
--*/
#include "common.h"

VOID
H4RxInit(_Out_ PH4_RX Rx, _In_ H4_RX_PACKET_CB OnPacket, _In_opt_ PVOID CbContext)
{
    RtlZeroMemory(Rx, sizeof(*Rx));
    Rx->State       = H4_WANT_TYPE;
    Rx->OnPacket    = OnPacket;
    Rx->OnPacketCtx = CbContext;
}

/* header byte count (after the type byte) for a given H4 type; 0 = unknown */
static ULONG H4HeaderLen(UCHAR Type)
{
    switch (Type) {
    case H4_CMD: return 3;   /* opcode2 + len1 */
    case H4_ACL: return 4;   /* handle2 + len2 */
    case H4_SCO: return 3;   /* handle2 + len1 */
    case H4_EVT: return 2;   /* code1   + len1 */
    default:     return 0;
    }
}

/* payload length, decoded from a fully-read header of the current type */
static ULONG H4PayloadLen(UCHAR Type, const UCHAR *Hdr)
{
    switch (Type) {
    case H4_CMD: return Hdr[2];
    case H4_ACL: return (ULONG)Hdr[2] | ((ULONG)Hdr[3] << 8);
    case H4_SCO: return Hdr[2];
    case H4_EVT: return Hdr[1];
    default:     return 0;
    }
}

static VOID H4Emit(PH4_RX Rx)
{
    if (Rx->OnPacket != NULL) {
        Rx->OnPacket(Rx->OnPacketCtx, Rx->Packet, Rx->PacketLen);
    }
    Rx->State = H4_WANT_TYPE;
}

/*
 * Feed received bytes. Calls OnPacket once per complete HCI packet, where the
 * packet buffer is [type][header...][payload...]. Returns the number of
 * complete packets emitted. Oversized payloads (> H4_RX_MAX_PAYLOAD) or an
 * unknown type byte resync the machine (the bad byte is dropped).
 */
ULONG
H4RxFeed(_Inout_ PH4_RX Rx, _In_reads_(Len) const UCHAR *Data, _In_ ULONG Len)
{
    ULONG emitted = 0;
    ULONG i;

    for (i = 0; i < Len; i++) {
        UCHAR b = Data[i];

        switch (Rx->State) {
        case H4_WANT_TYPE:
            Rx->HdrNeed = H4HeaderLen(b);
            if (Rx->HdrNeed == 0) {
                /* unknown type byte - drop and stay hunting for a valid type */
                break;
            }
            Rx->Type        = b;
            Rx->Packet[0]   = b;
            Rx->PacketLen   = 1;
            Rx->HdrGot      = 0;
            Rx->State       = H4_WANT_HEADER;
            break;

        case H4_WANT_HEADER:
            Rx->Hdr[Rx->HdrGot++] = b;
            Rx->Packet[Rx->PacketLen++] = b;
            if (Rx->HdrGot == Rx->HdrNeed) {
                Rx->PayloadNeed = H4PayloadLen(Rx->Type, Rx->Hdr);
                Rx->PayloadGot  = 0;
                if (Rx->PayloadNeed > H4_RX_MAX_PAYLOAD) {
                    /* malformed / too large - resync */
                    Rx->State = H4_WANT_TYPE;
                    break;
                }
                if (Rx->PayloadNeed == 0) {
                    H4Emit(Rx);
                    emitted++;
                } else {
                    Rx->State = H4_WANT_PAYLOAD;
                }
            }
            break;

        case H4_WANT_PAYLOAD:
            Rx->Packet[Rx->PacketLen++] = b;
            if (++Rx->PayloadGot == Rx->PayloadNeed) {
                H4Emit(Rx);
                emitted++;
            }
            break;

        default:
            Rx->State = H4_WANT_TYPE;
            break;
        }
    }
    return emitted;
}

/*
 * Convenience: is this assembled packet an HCI Command Complete event for the
 * given opcode? (event 0x0E: [0x04][0x0E][plen][numCmd][op_lo][op_hi][status...])
 * Returns 1 and writes *Status if so, else 0. Used by the init state machine to
 * confirm each vendor command before sending the next.
 */
ULONG
H4IsCommandComplete(_In_reads_(Len) const UCHAR *Pkt, _In_ ULONG Len,
                    _In_ USHORT Opcode, _Out_opt_ PUCHAR Status)
{
    USHORT op;
    if (Len < 7 || Pkt[0] != H4_EVT || Pkt[1] != 0x0E) {
        return 0;
    }
    op = (USHORT)Pkt[4] | ((USHORT)Pkt[5] << 8);
    if (op != Opcode) {
        return 0;
    }
    if (Status != NULL) {
        *Status = Pkt[6];
    }
    return 1;
}
