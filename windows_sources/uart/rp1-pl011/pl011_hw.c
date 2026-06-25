/*++
Module Name: pl011_hw.c
Abstract:    ARM PL011 UART register-level logic. See pl011_hw.h.
--*/
#include "pl011_hw.h"

void
Pl011BaudDivisor(_In_ ULONG UartClkHz, _In_ ULONG Baud, _Out_ ULONG *Ibrd, _Out_ ULONG *Fbrd)
{
    ULONG quot;

    if (Baud == 0) {
        *Ibrd = 0;
        *Fbrd = 0;
        return;
    }
    /* DIV_ROUND_CLOSEST(uartclk * 4, baud); quot is the divisor in 1/64 units */
    quot = (ULONG)(((unsigned long long)UartClkHz * 4u + (Baud / 2u)) / Baud);
    *Ibrd = quot >> 6;
    *Fbrd = quot & 0x3Fu;
}

ULONG
Pl011BuildLcrh(_In_ ULONG DataBits, _In_ ULONG StopBits, _In_ int ParityEnable,
               _In_ int ParityEven, _In_ int FifoEnable)
{
    ULONG wlen = (DataBits >= 5 && DataBits <= 8) ? (DataBits - 5u) : 3u;  /* 8->3 */
    ULONG lcrh = wlen << PL011_LCRH_WLEN_SHIFT;

    if (FifoEnable) lcrh |= PL011_LCRH_FEN;
    if (StopBits == 2) lcrh |= PL011_LCRH_STP2;
    if (ParityEnable) {
        lcrh |= PL011_LCRH_PEN;
        if (ParityEven) lcrh |= PL011_LCRH_EPS;
    }
    return lcrh;
}

ULONG
Pl011BuildCr(_In_ int TxEnable, _In_ int RxEnable, _In_ int FlowControl)
{
    ULONG cr = PL011_CR_UARTEN;
    if (TxEnable) cr |= PL011_CR_TXE;
    if (RxEnable) cr |= PL011_CR_RXE;
    if (FlowControl) cr |= PL011_CR_RTSEN | PL011_CR_CTSEN;
    return cr;
}

int Pl011TxFull(_In_ ULONG Fr)  { return (Fr & PL011_FR_TXFF) ? 1 : 0; }
int Pl011RxEmpty(_In_ ULONG Fr) { return (Fr & PL011_FR_RXFE) ? 1 : 0; }
int Pl011Busy(_In_ ULONG Fr)    { return (Fr & PL011_FR_BUSY) ? 1 : 0; }
