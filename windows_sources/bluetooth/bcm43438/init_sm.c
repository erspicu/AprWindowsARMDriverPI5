/*++
Module Name: init_sm.c
Abstract:    BCM4345C0 bring-up state machine (event-driven, hardware-independent).
             Ties together the H4 command framing (hci.c), the .hcd record parser
             and the vendor payload builders into the full bring-up order, one
             step per HCI Command Complete. Transport is injected (BTI_TX_FN), so
             the whole sequence is x64-sim verifiable (BTBCM_SIM). The driver
             wires Tx to the UART write and calls BtBcmInitOnEvent from the H4 RX
             pump. Order (Linux hci_bcm bcm_setup):
               Reset -> Download_Minidriver -> .hcd(Write_RAM)* -> Launch_RAM
                     -> Reset -> Write_BD_ADDR -> Update_Baud_Rate -> done
--*/
#include "common.h"

static int Bti_Send(PBTI S, USHORT Opcode, const UCHAR *Params, UCHAR ParamLen)
{
    UCHAR buf[300];
    ULONG n = BtBcmBuildCommand(buf, sizeof(buf), Opcode, (PUCHAR)Params, ParamLen);
    if (n == 0) { S->Error = 1; return -1; }
    S->Pending = Opcode;
    return S->Tx(S->TxCtx, buf, n);
}

/* send the next .hcd Write_RAM record; if none remain, transition to Launch_RAM */
static void Bti_SendNextHcdOrLaunch(PBTI S)
{
    UCHAR buf[300];
    ULONG consumed = 0;
    ULONG n = BtBcmHcdNextCommand(S->Hcd, S->HcdLen, S->HcdOff, buf, sizeof(buf), &consumed);

    if (n == 0) {
        UCHAR launch[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
        S->Step = BTI_LAUNCH;
        Bti_Send(S, BCM_OP_LAUNCH_RAM, launch, 4);
        return;
    }
    /* the record is already a framed H4 command; pending opcode is in buf[1..2] */
    S->Pending = (USHORT)buf[1] | ((USHORT)buf[2] << 8);
    S->HcdOff += consumed;
    (void)S->Tx(S->TxCtx, buf, n);
}

VOID
BtBcmInitStart(_Out_ PBTI S, _In_ ULONG Baud, _In_reads_(6) const UCHAR *Mac,
               _In_reads_(HcdLen) const UCHAR *Hcd, _In_ ULONG HcdLen,
               _In_ BTI_TX_FN Tx, _In_opt_ PVOID TxCtx)
{
    RtlZeroMemory(S, sizeof(*S));
    S->Baud   = Baud;
    S->BdAddr[0]=Mac[0]; S->BdAddr[1]=Mac[1]; S->BdAddr[2]=Mac[2];
    S->BdAddr[3]=Mac[3]; S->BdAddr[4]=Mac[4]; S->BdAddr[5]=Mac[5];
    S->Hcd    = Hcd;
    S->HcdLen = HcdLen;
    S->Tx     = Tx;
    S->TxCtx  = TxCtx;
    S->Step   = BTI_RESET1;
    Bti_Send(S, HCI_OP_RESET, NULL, 0);          /* kick off */
}

VOID
BtBcmInitOnEvent(_Inout_ PBTI S, _In_reads_(Len) const UCHAR *Pkt, _In_ ULONG Len)
{
    UCHAR status = 0xFF;

    if (S->Done || S->Error) {
        return;
    }
    /* only advance on the Command Complete for the opcode we are waiting on */
    if (!H4IsCommandComplete(Pkt, Len, S->Pending, &status)) {
        return;
    }
    if (status != 0) {
        S->Error = 1;
        return;
    }

    switch (S->Step) {
    case BTI_RESET1:
        S->Step = BTI_DOWNLOAD;
        Bti_Send(S, BCM_OP_DOWNLOAD_MINIDRV, NULL, 0);
        break;
    case BTI_DOWNLOAD:
        S->Step = BTI_HCD;
        Bti_SendNextHcdOrLaunch(S);
        break;
    case BTI_HCD:
        Bti_SendNextHcdOrLaunch(S);              /* next record, or -> Launch */
        break;
    case BTI_LAUNCH:
        S->Step = BTI_RESET2;
        Bti_Send(S, HCI_OP_RESET, NULL, 0);
        break;
    case BTI_RESET2: {
        UCHAR bd[6];
        BtBcmBuildBdAddrPayload(S->BdAddr, bd);
        S->Step = BTI_SETBDADDR;
        Bti_Send(S, BCM_OP_SET_BDADDR, bd, 6);
        break;
    }
    case BTI_SETBDADDR: {
        UCHAR br[6];
        BtBcmBuildBaudRatePayload(S->Baud, br);
        S->Step = BTI_SETBAUD;
        Bti_Send(S, BCM_OP_SET_BAUD_RATE, br, 6);
        break;
    }
    case BTI_SETBAUD:
        S->Step = BTI_DONE;
        S->Done = 1;
        break;
    default:
        break;
    }
}
