/*++
Module Name: pl011_hw.h
Abstract:    ARM PL011 UART hardware layer for the RP1/Pi5 UARTs (the bus the
             Bluetooth chip + serial console sit on). Pure register-level logic
             for a Windows SerCx2 controller driver: baud divisor (IBRD/FBRD),
             line-control (LCRH) and control (CR) word builders, flag-register
             decoders. Offsets/fields/formula calibrated from the Linux source
             (include/linux/amba/serial.h + amba-pl011.c). OS-independent;
             x64-sim verified (PL011_SIM).
--*/
#pragma once

#ifdef PL011_SIM
#include "sim/pl011_simshim.h"
#else
#include <ntddk.h>
#endif

/* ---- register offsets ---- */
#define PL011_DR      0x00u   /* data */
#define PL011_FR      0x18u   /* flag (RO) */
#define PL011_IBRD    0x24u   /* integer baud rate divisor */
#define PL011_FBRD    0x28u   /* fractional baud rate divisor */
#define PL011_LCRH    0x2Cu   /* line control */
#define PL011_CR      0x30u   /* control */
#define PL011_IFLS    0x34u   /* interrupt FIFO level select */
#define PL011_IMSC    0x38u   /* interrupt mask set/clear */
#define PL011_RIS     0x3Cu   /* raw interrupt status */
#define PL011_MIS     0x40u   /* masked interrupt status */
#define PL011_ICR     0x44u   /* interrupt clear */
#define PL011_DMACR   0x48u   /* DMA control */

/* ---- FR (flag register) bits ---- */
#define PL011_FR_TXFE  (1u << 7)   /* TX FIFO empty */
#define PL011_FR_RXFF  (1u << 6)   /* RX FIFO full */
#define PL011_FR_TXFF  (1u << 5)   /* TX FIFO full */
#define PL011_FR_RXFE  (1u << 4)   /* RX FIFO empty */
#define PL011_FR_BUSY  (1u << 3)   /* UART busy (TX in progress) */
#define PL011_FR_CTS   (1u << 0)

/* ---- LCRH (line control) bits ---- */
#define PL011_LCRH_WLEN_SHIFT  5
#define PL011_LCRH_FEN  (1u << 4)  /* FIFO enable */
#define PL011_LCRH_STP2 (1u << 3)  /* 2 stop bits */
#define PL011_LCRH_EPS  (1u << 2)  /* even parity select */
#define PL011_LCRH_PEN  (1u << 1)  /* parity enable */

/* ---- CR (control) bits ---- */
#define PL011_CR_UARTEN (1u << 0)
#define PL011_CR_TXE    (1u << 8)
#define PL011_CR_RXE    (1u << 9)
#define PL011_CR_RTSEN  (1u << 14)
#define PL011_CR_CTSEN  (1u << 15)

/* Compute IBRD/FBRD for the given UART input clock + baud (oversampling 16):
   quot = round(uartclk*4 / baud); IBRD = quot>>6; FBRD = quot & 0x3f.
   (amba-pl011.c set_termios). e.g. 48MHz/115200 -> IBRD=26, FBRD=3. */
void Pl011BaudDivisor(_In_ ULONG UartClkHz, _In_ ULONG Baud,
                      _Out_ ULONG *Ibrd, _Out_ ULONG *Fbrd);

/* Build the LCRH word: DataBits 5..8, StopBits 1 or 2, parity, FIFO enable. */
ULONG Pl011BuildLcrh(_In_ ULONG DataBits, _In_ ULONG StopBits, _In_ int ParityEnable,
                     _In_ int ParityEven, _In_ int FifoEnable);

/* Build the CR word: UARTEN + optional TX/RX/flow control. */
ULONG Pl011BuildCr(_In_ int TxEnable, _In_ int RxEnable, _In_ int FlowControl);

/* FR decoders */
int Pl011TxFull(_In_ ULONG Fr);
int Pl011RxEmpty(_In_ ULONG Fr);
int Pl011Busy(_In_ ULONG Fr);
