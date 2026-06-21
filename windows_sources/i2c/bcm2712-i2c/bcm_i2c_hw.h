/*++
Module Name: bcm_i2c_hw.h
Abstract:    BCM2835/BCM2712 BSC I2C HAL. Reusable core ported from Linux
             drivers/i2c/busses/i2c-bcm2835.c. Register map + polled xfer.
--*/
#pragma once
#ifdef BCMI2C_SIM
#include "sim/bcmi2c_simshim.h"   /* x64 user-mode test shim */
#else
#include <ntddk.h>
#endif

/* BSC register offsets (i2c-bcm2835.c) */
/* BCM2712 BSC I2C core clock = 108 MHz (Pi5-measured: clk-108M). */
#define BCM_I2C_CORE_HZ 108000000u

#define BCM_I2C_C       0x00   /* control */
#define BCM_I2C_S       0x04   /* status */
#define BCM_I2C_DLEN    0x08   /* data length */
#define BCM_I2C_A       0x0c   /* slave address */
#define BCM_I2C_FIFO    0x10   /* data FIFO */
#define BCM_I2C_DIV     0x14   /* clock divider */
#define BCM_I2C_DEL     0x18
#define BCM_I2C_CLKT    0x1c

/* C bits */
#define BCM_I2C_C_READ  (1u << 0)
#define BCM_I2C_C_CLEAR (1u << 4)
#define BCM_I2C_C_ST    (1u << 7)
#define BCM_I2C_C_I2CEN (1u << 15)

/* S bits */
#define BCM_I2C_S_TA    (1u << 0)
#define BCM_I2C_S_DONE  (1u << 1)
#define BCM_I2C_S_TXW   (1u << 2)
#define BCM_I2C_S_RXR   (1u << 3)
#define BCM_I2C_S_TXD   (1u << 4)
#define BCM_I2C_S_RXD   (1u << 5)
#define BCM_I2C_S_ERR   (1u << 8)
#define BCM_I2C_S_CLKT  (1u << 9)

typedef struct _BCM_I2C_HW {
    PUCHAR  Base;
    SIZE_T  Length;
    UINT16  TargetAddr;
} BCM_I2C_HW, *PBCM_I2C_HW;

FORCEINLINE UINT32 BcmI2cRead(_In_ PBCM_I2C_HW Hw, _In_ ULONG Reg)
{ return READ_REGISTER_ULONG((volatile ULONG *)(Hw->Base + Reg)); }
FORCEINLINE VOID BcmI2cWrite(_In_ PBCM_I2C_HW Hw, _In_ ULONG Reg, _In_ UINT32 Val)
{ WRITE_REGISTER_ULONG((volatile ULONG *)(Hw->Base + Reg), Val); }

VOID     BcmI2cHwInit(_Out_ PBCM_I2C_HW Hw, _In_ PUCHAR Base, _In_ SIZE_T Length);
UINT16   BcmI2cHwDivForHz(_In_ UINT32 SclHz);   /* DIV from target SCL @ core=108MHz */
VOID     BcmI2cHwSetDivider(_Inout_ PBCM_I2C_HW Hw, _In_ UINT16 Div);
VOID     BcmI2cHwSetTarget(_Inout_ PBCM_I2C_HW Hw, _In_ UINT16 Addr7);
NTSTATUS BcmI2cHwWritePolled(_Inout_ PBCM_I2C_HW Hw, _In_reads_(Len) const UCHAR *Buf, _In_ ULONG Len);
NTSTATUS BcmI2cHwReadPolled(_Inout_ PBCM_I2C_HW Hw, _Out_writes_(Len) UCHAR *Buf, _In_ ULONG Len);
