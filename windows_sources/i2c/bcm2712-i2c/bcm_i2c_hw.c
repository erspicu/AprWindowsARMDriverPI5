/*++
Module Name: bcm_i2c_hw.c
Abstract:    BCM2835/BCM2712 BSC I2C HAL (polled). Ported from i2c-bcm2835.c.
--*/
#include "bcm_i2c_hw.h"

#define BCM_I2C_POLL_SPINS   100000

VOID BcmI2cHwInit(_Out_ PBCM_I2C_HW Hw, _In_ PUCHAR Base, _In_ SIZE_T Length)
{
    RtlZeroMemory(Hw, sizeof(*Hw));
    Hw->Base = Base;
    Hw->Length = Length;
}

/* DIV from target SCL from the BCM2712 BSC core clock = 108 MHz (Pi5-measured:
 * clk-108M / "108MHz-clock"). SCL = core / DIV. NOTE: the prior 0x5dc default
 * assumed 150 MHz (gave only ~72 kHz at the real 108 MHz). */
UINT16 BcmI2cHwDivForHz(_In_ UINT32 SclHz)
{
    UINT32 div;
    if (SclHz == 0) return 0x0438;                  /* 1080 -> 100 kHz @108MHz */
    div = (BCM_I2C_CORE_HZ + SclHz - 1) / SclHz;    /* ceil */
    if (div > 0xFFFE) div = 0xFFFE;
    if (div < 2)      div = 2;
    return (UINT16)div;
}

VOID BcmI2cHwSetDivider(_Inout_ PBCM_I2C_HW Hw, _In_ UINT16 Div)
{
    /* 0x438 = 1080 = 108 MHz / 100 kHz (was 0x5dc, which assumed 150 MHz). */
    BcmI2cWrite(Hw, BCM_I2C_DIV, Div ? Div : 0x0438);
}

VOID BcmI2cHwSetTarget(_Inout_ PBCM_I2C_HW Hw, _In_ UINT16 Addr7)
{
    Hw->TargetAddr = Addr7 & 0x7f;
}

static VOID BcmI2cBegin(_Inout_ PBCM_I2C_HW Hw, _In_ ULONG Len, _In_ UINT32 ReadBit)
{
    BcmI2cWrite(Hw, BCM_I2C_C, BCM_I2C_C_I2CEN | BCM_I2C_C_CLEAR);   /* clear FIFO */
    BcmI2cWrite(Hw, BCM_I2C_S, BCM_I2C_S_DONE | BCM_I2C_S_ERR | BCM_I2C_S_CLKT);
    BcmI2cWrite(Hw, BCM_I2C_A, Hw->TargetAddr);
    BcmI2cWrite(Hw, BCM_I2C_DLEN, Len);
    BcmI2cWrite(Hw, BCM_I2C_C, BCM_I2C_C_I2CEN | BCM_I2C_C_ST | ReadBit);
}

static NTSTATUS BcmI2cWaitDone(_Inout_ PBCM_I2C_HW Hw)
{
    ULONG s;
    for (s = 0; s < BCM_I2C_POLL_SPINS; s++) {
        UINT32 st = BcmI2cRead(Hw, BCM_I2C_S);
        if (st & (BCM_I2C_S_ERR | BCM_I2C_S_CLKT)) return STATUS_IO_DEVICE_ERROR;
        if (st & BCM_I2C_S_DONE) return STATUS_SUCCESS;
        KeStallExecutionProcessor(1);
    }
    return STATUS_IO_TIMEOUT;
}

NTSTATUS BcmI2cHwWritePolled(_Inout_ PBCM_I2C_HW Hw, _In_reads_(Len) const UCHAR *Buf, _In_ ULONG Len)
{
    ULONG i = 0;
    ULONG spins = 0;

    BcmI2cBegin(Hw, Len, 0);
    while (i < Len) {
        UINT32 st = BcmI2cRead(Hw, BCM_I2C_S);
        if (st & (BCM_I2C_S_ERR | BCM_I2C_S_CLKT)) return STATUS_IO_DEVICE_ERROR;
        if (st & BCM_I2C_S_TXD) {
            BcmI2cWrite(Hw, BCM_I2C_FIFO, Buf[i]);
            i++; spins = 0;
        } else if (++spins > BCM_I2C_POLL_SPINS) {
            return STATUS_IO_TIMEOUT;
        } else {
            KeStallExecutionProcessor(1);
        }
    }
    return BcmI2cWaitDone(Hw);
}

NTSTATUS BcmI2cHwReadPolled(_Inout_ PBCM_I2C_HW Hw, _Out_writes_(Len) UCHAR *Buf, _In_ ULONG Len)
{
    ULONG i = 0;
    ULONG spins = 0;

    BcmI2cBegin(Hw, Len, BCM_I2C_C_READ);
    while (i < Len) {
        UINT32 st = BcmI2cRead(Hw, BCM_I2C_S);
        if (st & (BCM_I2C_S_ERR | BCM_I2C_S_CLKT)) return STATUS_IO_DEVICE_ERROR;
        if (st & BCM_I2C_S_RXD) {
            Buf[i] = (UCHAR)(BcmI2cRead(Hw, BCM_I2C_FIFO) & 0xff);
            i++; spins = 0;
        } else if (st & BCM_I2C_S_DONE) {
            break;
        } else if (++spins > BCM_I2C_POLL_SPINS) {
            return STATUS_IO_TIMEOUT;
        } else {
            KeStallExecutionProcessor(1);
        }
    }
    return STATUS_SUCCESS;
}
