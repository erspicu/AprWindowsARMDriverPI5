/*++
Module Name: rp1_gpio_hw.h
Abstract:    RP1 GPIO hardware layer (ported from pinctrl-rp1.c). Pin read/write
             via the RIO block (OUT/OE/IN + atomic SET/CLR aliases) and function
             select via the per-pin CTRL register.

    RP1 has 3 banks: pins 0-27, 28-33, 34-53. Each bank's RIO/GPIO sub-block is
    at bank*0x4000. GpioClx presents a single 54-pin bank; we map pin->bank here.
--*/
#pragma once
#ifdef GPIO_SIM
#include "sim/gpio_simshim.h"   /* x64 user-mode test shim */
#else
#include <ntddk.h>
#endif

/* RIO atomic aliases (relative to a register block) */
#define RP1_RW_OFFSET    0x0000
#define RP1_SET_OFFSET   0x2000
#define RP1_CLR_OFFSET   0x3000

/* RIO register block (register I/O) */
#define RP1_RIO_OUT      0x00
#define RP1_RIO_OE       0x04
#define RP1_RIO_IN       0x08

/* per-pin status/ctrl in the GPIO io block (stride 8 bytes) */
#define RP1_GPIO_CTRL    0x04
#define RP1_GPIO_PIN_STRIDE       8
#define RP1_GPIO_CTRL_FUNCSEL_MASK 0x1f
#define RP1_FSEL_GPIO    0x05
/* CTRL output/output-enable override fields: PERI(0) lets the RIO drive the pin */
#define RP1_GPIO_CTRL_OUTOVER_MASK 0x00003000   /* [13:12] */
#define RP1_GPIO_CTRL_OEOVER_MASK  0x0000c000   /* [15:14] */

/* PADS block: per-bank base 0x04 (skips the bank voltage-select reg), stride 4.
 * pad(pin) = PadsBase + bank*0x4000 + 0x04 + off*4 (rp1_iobanks[].pads_offset). */
#define RP1_PAD_BANK_BASE   0x04
#define RP1_PAD_PULL_LSB    2
#define RP1_PAD_PULL_MASK   0x0000000c
#define RP1_PUD_OFF         0
#define RP1_PUD_DOWN        1
#define RP1_PUD_UP          2
#define RP1_PAD_IN_ENABLE   0x00000040   /* bit6: input buffer enable */
#define RP1_PAD_OUT_DISABLE 0x00000080   /* bit7: output buffer disable */

#define RP1_BANK_STRIDE  0x4000

/* per-bank PCIe interrupt enable/status (aggregated routing to RP1 MSI-X) */
#define RP1_GPIO_PCIE_INTE  0x011c
#define RP1_GPIO_PCIE_INTS  0x0124
/* per-pin CTRL interrupt-enable + latch-reset bits */
#define RP1_GPIO_CTRL_IRQEN_FALLING (1u << 20)
#define RP1_GPIO_CTRL_IRQEN_RISING  (1u << 21)
#define RP1_GPIO_CTRL_IRQEN_LOW     (1u << 22)
#define RP1_GPIO_CTRL_IRQEN_HIGH    (1u << 23)
#define RP1_GPIO_CTRL_IRQRESET      (1u << 28)
#define RP1_GPIO_CTRL_IRQEN_MASK \
    (RP1_GPIO_CTRL_IRQEN_FALLING | RP1_GPIO_CTRL_IRQEN_RISING | \
     RP1_GPIO_CTRL_IRQEN_LOW | RP1_GPIO_CTRL_IRQEN_HIGH)

typedef struct _RP1_GPIO_HW {
    PUCHAR GpioBase;    // per-pin status/ctrl region (3 banks at bank*0x4000)
    PUCHAR RioBase;     // register I/O region (OUT/OE/IN + atomic aliases)
    PUCHAR PadsBase;
    SIZE_T GpioLen, RioLen, PadsLen;
} RP1_GPIO_HW, *PRP1_GPIO_HW;

// pin -> bank (0..2) + bank-relative offset; returns -1 if out of range.
int  Rp1GpioPinToBank(_In_ ULONG Pin, _Out_ ULONG *Offset);

BOOLEAN Rp1GpioReadPin(_In_ PRP1_GPIO_HW Hw, _In_ ULONG Pin);
VOID    Rp1GpioWritePin(_In_ PRP1_GPIO_HW Hw, _In_ ULONG Pin, _In_ BOOLEAN Value);
VOID    Rp1GpioSetDirection(_In_ PRP1_GPIO_HW Hw, _In_ ULONG Pin, _In_ BOOLEAN Output);
VOID    Rp1GpioSelectGpioFunction(_In_ PRP1_GPIO_HW Hw, _In_ ULONG Pin);
VOID    Rp1GpioSetPull(_In_ PRP1_GPIO_HW Hw, _In_ ULONG Pin, _In_ ULONG Pud);  /* RP1_PUD_* */

/* interrupt control (per-pin CTRL IRQEN + per-bank PCIe INTE/INTS) */
VOID  Rp1GpioEnableIrq(_In_ PRP1_GPIO_HW Hw, _In_ ULONG Pin,
                       _In_ BOOLEAN Rising, _In_ BOOLEAN Falling,
                       _In_ BOOLEAN High, _In_ BOOLEAN Low);
VOID  Rp1GpioMaskIrq(_In_ PRP1_GPIO_HW Hw, _In_ ULONG Pin);
VOID  Rp1GpioUnmaskIrq(_In_ PRP1_GPIO_HW Hw, _In_ ULONG Pin);
ULONG Rp1GpioQueryActiveIrq(_In_ PRP1_GPIO_HW Hw, _In_ int Bank);
VOID  Rp1GpioAckIrq(_In_ PRP1_GPIO_HW Hw, _In_ ULONG Pin);
