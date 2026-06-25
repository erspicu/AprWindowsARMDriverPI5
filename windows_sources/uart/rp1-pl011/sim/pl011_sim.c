/*++ x64 sim of the PL011 HAL. Build: cl /DPL011_SIM /I.. pl011_sim.c ..\pl011_hw.c --*/
#include <stdio.h>
#include "../pl011_hw.h"

static int g_pass, g_fail;
static void check(const char *w, int c)
{ if (c) { g_pass++; printf("  [PASS] %s\n", w); } else { g_fail++; printf("  [FAIL] %s\n", w); } }

int main(void)
{
    ULONG ibrd, fbrd, lcrh, cr;

    printf("== PL011 UART HAL simulation ==\n");

    printf("-- baud divisor (uartclk 48 MHz) --\n");
    Pl011BaudDivisor(48000000u, 115200u, &ibrd, &fbrd);
    check("115200 -> IBRD 26", ibrd == 26);
    check("115200 -> FBRD 3",  fbrd == 3);
    Pl011BaudDivisor(48000000u, 9600u, &ibrd, &fbrd);
    check("9600 -> IBRD 312", ibrd == 312);   /* 48e6*4/9600=20000; >>6=312, &3f=32 */
    check("9600 -> FBRD 32",  fbrd == 32);
    Pl011BaudDivisor(48000000u, 3000000u, &ibrd, &fbrd);
    check("3M -> IBRD 1", ibrd == 1);          /* 192000000/3000000=64; >>6=1, &3f=0 */
    check("3M -> FBRD 0", fbrd == 0);
    Pl011BaudDivisor(48000000u, 0u, &ibrd, &fbrd);
    check("baud 0 guarded", ibrd == 0 && fbrd == 0);

    printf("-- LCRH --\n");
    lcrh = Pl011BuildLcrh(8, 1, 0, 0, 1);   /* 8N1 + FIFO */
    check("8N1+FIFO == 0x70", lcrh == 0x70);            /* WLEN8(0x60)|FEN(0x10) */
    lcrh = Pl011BuildLcrh(7, 2, 1, 1, 0);   /* 7E2, no FIFO */
    check("7E2 WLEN==2", ((lcrh >> 5) & 0x3) == 2);
    check("7E2 STP2 set", (lcrh & PL011_LCRH_STP2) != 0);
    check("7E2 PEN+EPS set", (lcrh & PL011_LCRH_PEN) && (lcrh & PL011_LCRH_EPS));
    check("7E2 FEN clear", (lcrh & PL011_LCRH_FEN) == 0);

    printf("-- CR --\n");
    cr = Pl011BuildCr(1, 1, 0);
    check("enable TX+RX == 0x301", cr == (PL011_CR_UARTEN | PL011_CR_TXE | PL011_CR_RXE));
    cr = Pl011BuildCr(1, 1, 1);
    check("flow control sets RTSEN+CTSEN", (cr & PL011_CR_RTSEN) && (cr & PL011_CR_CTSEN));

    printf("-- FR decoders --\n");
    check("TXFF -> TxFull", Pl011TxFull(PL011_FR_TXFF) == 1);
    check("no TXFF -> not full", Pl011TxFull(0) == 0);
    check("RXFE -> RxEmpty", Pl011RxEmpty(PL011_FR_RXFE) == 1);
    check("BUSY -> Busy", Pl011Busy(PL011_FR_BUSY) == 1);

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
