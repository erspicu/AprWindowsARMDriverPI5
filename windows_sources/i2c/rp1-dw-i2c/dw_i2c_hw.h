/*++
Module Name: dw_i2c_hw.h
Abstract:    Synopsys DesignWare I2C hardware abstraction layer (HAL).
             Reusable core ported from Linux drivers/i2c/busses/i2c-designware-*.
             Register map + master init/configure/enable + polled transfer.
             OS glue (SpbCx, interrupts, clocks) lives in the driver layer.
--*/
#pragma once
#ifdef DWI2C_SIM
#include "sim/dwi2c_simshim.h"   /* x64 user-mode test shim (fake kernel types + reg mock) */
#else
#include <ntddk.h>
#endif

/* ---- DesignWare I2C register offsets (from i2c-designware-core.h) ---- */
#define DW_IC_CON               0x00
#define DW_IC_TAR               0x04
#define DW_IC_DATA_CMD          0x10
#define DW_IC_SS_SCL_HCNT       0x14
#define DW_IC_SS_SCL_LCNT       0x18
#define DW_IC_FS_SCL_HCNT       0x1c
#define DW_IC_FS_SCL_LCNT       0x20
#define DW_IC_INTR_STAT         0x2c
#define DW_IC_INTR_MASK         0x30
#define DW_IC_RAW_INTR_STAT     0x34
#define DW_IC_RX_TL             0x38
#define DW_IC_TX_TL             0x3c
#define DW_IC_CLR_INTR          0x40
#define DW_IC_CLR_TX_ABRT       0x54
#define DW_IC_ENABLE            0x6c
#define DW_IC_STATUS            0x70
#define DW_IC_TXFLR             0x74
#define DW_IC_RXFLR             0x78
#define DW_IC_SDA_HOLD          0x7c
#define DW_IC_SDA_HOLD_RX_SHIFT 16
#define DW_IC_SDA_HOLD_RX_MASK  0x00FF0000u   /* SDA_HOLD[23:16] = RX hold */
#define DW_IC_TX_ABRT_SOURCE    0x80
#define DW_IC_ENABLE_STATUS     0x9c
#define DW_IC_ENABLE_STATUS_IC_EN 0x1         /* bit0: controller still active */
#define DW_IC_COMP_PARAM_1      0xf4
#define DW_IC_COMP_TYPE         0xfc
#define DW_IC_COMP_TYPE_VALUE   0x44570140u   /* "DW" + 0x0140 */

/* CON bits */
#define DW_IC_CON_MASTER        (1u << 0)
#define DW_IC_CON_SPEED_STD     (1u << 1)
#define DW_IC_CON_SPEED_FAST    (2u << 1)
#define DW_IC_CON_SPEED_HIGH    (3u << 1)
#define DW_IC_CON_RESTART_EN    (1u << 5)
#define DW_IC_CON_SLAVE_DISABLE (1u << 6)

/* DATA_CMD bits */
#define DW_IC_DATA_CMD_READ     (1u << 8)
#define DW_IC_DATA_CMD_STOP     (1u << 9)
#define DW_IC_DATA_CMD_RESTART  (1u << 10)

/* STATUS bits */
#define DW_IC_STATUS_ACTIVITY   (1u << 0)
#define DW_IC_STATUS_TFE        (1u << 2)   /* tx fifo empty */
#define DW_IC_STATUS_RFNE       (1u << 3)   /* rx fifo not empty */
#define DW_IC_STATUS_TFNF       (1u << 1)   /* tx fifo not full */
#define DW_IC_STATUS_MST_ACT    (1u << 5)

/* ENABLE bits */
#define DW_IC_ENABLE_ENABLE     (1u << 0)

/* INTR bits */
#define DW_IC_INTR_TX_ABRT      (1u << 6)
#define DW_IC_INTR_STOP_DET     (1u << 9)

#define DW_IC_TAR_10BIT_MASTER  (1u << 12)

typedef enum _DW_I2C_SPEED {
    DwI2cStandard = 0,   /* 100 kHz */
    DwI2cFast,           /* 400 kHz */
    DwI2cHigh            /* 3.4 MHz */
} DW_I2C_SPEED;

typedef struct _DW_I2C_HW {
    PUCHAR  Base;
    SIZE_T  Length;
    UINT32  CompType;       /* cached COMP_TYPE (sanity) */
    UINT32  TxFifoDepth;
    UINT32  RxFifoDepth;
    UINT32  MasterCfg;      /* CON value */
    UINT16  TargetAddr;     /* current 7-bit slave address */
} DW_I2C_HW, *PDW_I2C_HW;

FORCEINLINE UINT32 DwI2cRead(_In_ PDW_I2C_HW Hw, _In_ ULONG Reg)
{ return READ_REGISTER_ULONG((volatile ULONG *)(Hw->Base + Reg)); }
FORCEINLINE VOID DwI2cWrite(_In_ PDW_I2C_HW Hw, _In_ ULONG Reg, _In_ UINT32 Val)
{ WRITE_REGISTER_ULONG((volatile ULONG *)(Hw->Base + Reg), Val); }

VOID     DwI2cHwInit(_Out_ PDW_I2C_HW Hw, _In_ PUCHAR Base, _In_ SIZE_T Length);
NTSTATUS DwI2cHwProbe(_Inout_ PDW_I2C_HW Hw);
VOID     DwI2cHwConfigureMaster(_Inout_ PDW_I2C_HW Hw, _In_ DW_I2C_SPEED Speed);
VOID     DwI2cHwEnable(_Inout_ PDW_I2C_HW Hw);
VOID     DwI2cHwDisable(_Inout_ PDW_I2C_HW Hw);
VOID     DwI2cHwSetTarget(_Inout_ PDW_I2C_HW Hw, _In_ UINT16 Addr7);
NTSTATUS DwI2cHwWritePolled(_Inout_ PDW_I2C_HW Hw, _In_reads_(Len) const UCHAR *Buf, _In_ ULONG Len, _In_ BOOLEAN Stop);
NTSTATUS DwI2cHwReadPolled(_Inout_ PDW_I2C_HW Hw, _Out_writes_(Len) UCHAR *Buf, _In_ ULONG Len, _In_ BOOLEAN Stop);
