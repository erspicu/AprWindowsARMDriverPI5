/*++
Module Name: common.h
Abstract:    BCM2712 BSC I2C controller driver (SpbCx) - shared decls.
             Same SpbCx pattern as the RP1 I2C/SPI drivers, with the BSC HAL.
--*/
#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <spb.h>
#include <spbcx.h>
#include "bcm_i2c_hw.h"

#define BCMI2C_POOLTAG  'cIcB'   // 'BcIc'

typedef struct _BCMI2C_DEVICE {
    BCM_I2C_HW Hw;
    PUCHAR     Mmio;
    SIZE_T     MmioLen;
} BCMI2C_DEVICE, *PBCMI2C_DEVICE;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BCMI2C_DEVICE, BcmI2cGetContext)

EVT_WDF_DRIVER_DEVICE_ADD            BcmI2cEvtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE      BcmI2cEvtPrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE      BcmI2cEvtReleaseHardware;
EVT_SPB_TARGET_CONNECT               BcmI2cEvtTargetConnect;
EVT_SPB_TARGET_DISCONNECT            BcmI2cEvtTargetDisconnect;
EVT_SPB_CONTROLLER_READ              BcmI2cEvtIoRead;
EVT_SPB_CONTROLLER_WRITE             BcmI2cEvtIoWrite;
EVT_SPB_CONTROLLER_SEQUENCE          BcmI2cEvtIoSequence;
