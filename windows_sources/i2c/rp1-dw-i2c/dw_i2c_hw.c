/*++
Module Name: dw_i2c_hw.c
Abstract:    DesignWare I2C HAL implementation (master, polled). Ported from
             Linux i2c-designware-common.c / i2c-designware-master.c logic.
--*/
#include "dw_i2c_hw.h"

#define DW_I2C_POLL_SPINS   100000   /* bounded poll iterations */

VOID DwI2cHwInit(_Out_ PDW_I2C_HW Hw, _In_ PUCHAR Base, _In_ SIZE_T Length)
{
    RtlZeroMemory(Hw, sizeof(*Hw));
    Hw->Base = Base;
    Hw->Length = Length;
    Hw->TxFifoDepth = 16;
    Hw->RxFifoDepth = 16;
}

/* port of i2c_dw_set_fifo_size() + COMP_TYPE sanity (i2c_dw_init_regmap) */
NTSTATUS DwI2cHwProbe(_Inout_ PDW_I2C_HW Hw)
{
    UINT32 comp1;

    Hw->CompType = DwI2cRead(Hw, DW_IC_COMP_TYPE);
    comp1 = DwI2cRead(Hw, DW_IC_COMP_PARAM_1);
    Hw->TxFifoDepth = ((comp1 >> 16) & 0xff) + 1;
    Hw->RxFifoDepth = ((comp1 >> 8) & 0xff) + 1;

    if (Hw->CompType != DW_IC_COMP_TYPE_VALUE) {
        return STATUS_DEVICE_CONFIGURATION_ERROR;   /* bogus MMIO / no HW */
    }
    return STATUS_SUCCESS;
}

/* port of i2c_dw_configure_master() + __i2c_dw_init() master setup */
VOID DwI2cHwConfigureMaster(_Inout_ PDW_I2C_HW Hw, _In_ DW_I2C_SPEED Speed)
{
    UINT32 speedBits;

    switch (Speed) {
    case DwI2cHigh: speedBits = DW_IC_CON_SPEED_HIGH; break;
    case DwI2cFast: speedBits = DW_IC_CON_SPEED_FAST; break;
    default:        speedBits = DW_IC_CON_SPEED_STD;  break;
    }
    Hw->MasterCfg = DW_IC_CON_MASTER | DW_IC_CON_SLAVE_DISABLE |
                    DW_IC_CON_RESTART_EN | speedBits;

    DwI2cHwDisable(Hw);
    DwI2cWrite(Hw, DW_IC_CON, Hw->MasterCfg);

    /* SCL high/low counts for IC clock = 200 MHz (RP1 clk_sys, measured on a
     * live Pi5 via clk_summary; RP1 DW-I2C is parented to clk_sys). Computed with
     * the DesignWare i2c_dw_scl_hcnt/lcnt formulas (ic_clk=200000 kHz, tf=300 ns):
     *   SS(100k): HCNT=(200000*4300+5e5)/1e6-3=857  LCNT=(200000*5000+5e5)/1e6-1=999
     *   FS(400k): HCNT=(200000*900 +5e5)/1e6-3=177  LCNT=(200000*1600+5e5)/1e6-1=319
     * See MD/Note/20260621-0720-pi5-linux-hardware-facts.md. */
    DwI2cWrite(Hw, DW_IC_SS_SCL_HCNT, 0x0359);   /* 857 */
    DwI2cWrite(Hw, DW_IC_SS_SCL_LCNT, 0x03E7);   /* 999 */
    DwI2cWrite(Hw, DW_IC_FS_SCL_HCNT, 0x00B1);   /* 177 */
    DwI2cWrite(Hw, DW_IC_FS_SCL_LCNT, 0x013F);   /* 319 */

    /* SDA hold time (i2c_dw_set_sda_hold). COMP_VERSION 0x140 >= min, so the RX-hold
     * workaround path applies: keep the prior TX hold, and if the RX-hold field is
     * zero set it to 1 (avoids TX arbitration-lost when a slave pulls SDA low too
     * quickly after SCL's falling edge). Must be written while the IC is disabled. */
    {
        UINT32 hold = DwI2cRead(Hw, DW_IC_SDA_HOLD);
        if (!(hold & DW_IC_SDA_HOLD_RX_MASK)) {
            hold |= 1u << DW_IC_SDA_HOLD_RX_SHIFT;
        }
        DwI2cWrite(Hw, DW_IC_SDA_HOLD, hold);
    }

    /* FIFO thresholds + polled mode (mask all interrupts) */
    DwI2cWrite(Hw, DW_IC_TX_TL, Hw->TxFifoDepth / 2);
    DwI2cWrite(Hw, DW_IC_RX_TL, 0);
    DwI2cWrite(Hw, DW_IC_INTR_MASK, 0);
}

VOID DwI2cHwEnable(_Inout_ PDW_I2C_HW Hw)
{
    DwI2cWrite(Hw, DW_IC_ENABLE, DW_IC_ENABLE_ENABLE);
}

VOID DwI2cHwDisable(_Inout_ PDW_I2C_HW Hw)
{
    UINT32 i;
    DwI2cWrite(Hw, DW_IC_ENABLE, 0);
    /* __i2c_dw_disable: poll IC_ENABLE_STATUS until the controller actually quiesces.
     * Without this, an immediate reconfigure (e.g. DwI2cHwSetTarget's disable->write
     * TAR->enable) can land while the IC is still busy and be silently ignored. */
    for (i = 0; i < 100; i++) {
        if (!(DwI2cRead(Hw, DW_IC_ENABLE_STATUS) & DW_IC_ENABLE_STATUS_IC_EN)) {
            break;
        }
    }
}

VOID DwI2cHwSetTarget(_Inout_ PDW_I2C_HW Hw, _In_ UINT16 Addr7)
{
    DwI2cHwDisable(Hw);
    DwI2cWrite(Hw, DW_IC_TAR, Addr7 & 0x7f);   /* 7-bit target address */
    Hw->TargetAddr = Addr7 & 0x7f;
    DwI2cHwEnable(Hw);
}

static NTSTATUS DwI2cWaitTxNotFull(_Inout_ PDW_I2C_HW Hw)
{
    ULONG spins;
    for (spins = 0; spins < DW_I2C_POLL_SPINS; spins++) {
        if (DwI2cRead(Hw, DW_IC_STATUS) & DW_IC_STATUS_TFNF) {
            return STATUS_SUCCESS;
        }
        KeStallExecutionProcessor(1);
    }
    return STATUS_IO_TIMEOUT;
}

static NTSTATUS DwI2cWaitRxNotEmpty(_Inout_ PDW_I2C_HW Hw)
{
    ULONG spins;
    for (spins = 0; spins < DW_I2C_POLL_SPINS; spins++) {
        if (DwI2cRead(Hw, DW_IC_RAW_INTR_STAT) & DW_IC_INTR_TX_ABRT) {
            return STATUS_IO_DEVICE_ERROR;
        }
        if (DwI2cRead(Hw, DW_IC_STATUS) & DW_IC_STATUS_RFNE) {
            return STATUS_SUCCESS;
        }
        KeStallExecutionProcessor(1);
    }
    return STATUS_IO_TIMEOUT;
}

NTSTATUS DwI2cHwWritePolled(_Inout_ PDW_I2C_HW Hw, _In_reads_(Len) const UCHAR *Buf,
                            _In_ ULONG Len, _In_ BOOLEAN Stop)
{
    ULONG i;
    NTSTATUS status;
    for (i = 0; i < Len; i++) {
        UINT32 cmd = Buf[i];
        if (Stop && (i == Len - 1)) {
            cmd |= DW_IC_DATA_CMD_STOP;
        }
        status = DwI2cWaitTxNotFull(Hw);
        if (!NT_SUCCESS(status)) return status;
        DwI2cWrite(Hw, DW_IC_DATA_CMD, cmd);
    }
    if (DwI2cRead(Hw, DW_IC_RAW_INTR_STAT) & DW_IC_INTR_TX_ABRT) {
        (VOID)DwI2cRead(Hw, DW_IC_CLR_TX_ABRT);
        return STATUS_IO_DEVICE_ERROR;
    }
    return STATUS_SUCCESS;
}

NTSTATUS DwI2cHwReadPolled(_Inout_ PDW_I2C_HW Hw, _Out_writes_(Len) UCHAR *Buf,
                           _In_ ULONG Len, _In_ BOOLEAN Stop)
{
    ULONG i;
    NTSTATUS status;
    for (i = 0; i < Len; i++) {
        UINT32 cmd = DW_IC_DATA_CMD_READ;
        if (Stop && (i == Len - 1)) {
            cmd |= DW_IC_DATA_CMD_STOP;
        }
        status = DwI2cWaitTxNotFull(Hw);
        if (!NT_SUCCESS(status)) return status;
        DwI2cWrite(Hw, DW_IC_DATA_CMD, cmd);     /* issue read request */

        status = DwI2cWaitRxNotEmpty(Hw);
        if (!NT_SUCCESS(status)) return status;
        Buf[i] = (UCHAR)(DwI2cRead(Hw, DW_IC_DATA_CMD) & 0xff);
    }
    return STATUS_SUCCESS;
}
