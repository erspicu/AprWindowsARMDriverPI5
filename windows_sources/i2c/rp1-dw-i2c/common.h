/*++
Module Name: common.h
Abstract:    RP1 DesignWare I2C controller driver (SpbCx) - shared decls.
             SpbCx (Simple Peripheral Bus class extension) presents this I2C
             controller to Windows; client drivers issue I2C reads/writes/
             sequences which we service via the ported DesignWare HAL.
--*/
#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <spb.h>
#include <spbcx.h>
#include "dw_i2c_hw.h"

#define RP1I2C_POOLTAG  'cI1R'   // 'R1Ic'

typedef struct _RP1I2C_DEVICE {
    DW_I2C_HW Hw;
    PUCHAR    Mmio;
    SIZE_T    MmioLen;
} RP1I2C_DEVICE, *PRP1I2C_DEVICE;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(RP1I2C_DEVICE, Rp1I2cGetContext)

EVT_WDF_DRIVER_DEVICE_ADD            Rp1I2cEvtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE      Rp1I2cEvtPrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE      Rp1I2cEvtReleaseHardware;
EVT_SPB_TARGET_CONNECT               Rp1I2cEvtTargetConnect;
EVT_SPB_TARGET_DISCONNECT            Rp1I2cEvtTargetDisconnect;
EVT_SPB_CONTROLLER_READ              Rp1I2cEvtIoRead;
EVT_SPB_CONTROLLER_WRITE             Rp1I2cEvtIoWrite;
EVT_SPB_CONTROLLER_SEQUENCE          Rp1I2cEvtIoSequence;
