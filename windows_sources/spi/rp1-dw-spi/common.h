/*++
Module Name: common.h
Abstract:    RP1 DesignWare SPI controller driver (SpbCx) - shared decls.
             Same SpbCx pattern as the RP1 I2C driver, with the DW-SSI HAL.
--*/
#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <spb.h>
#include <spbcx.h>
#include "dw_spi_hw.h"

#define RP1SPI_POOLTAG  'pS1R'   // 'R1Sp'

typedef struct _RP1SPI_DEVICE {
    DW_SPI_HW Hw;
    PUCHAR    Mmio;
    SIZE_T    MmioLen;
} RP1SPI_DEVICE, *PRP1SPI_DEVICE;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(RP1SPI_DEVICE, Rp1SpiGetContext)

EVT_WDF_DRIVER_DEVICE_ADD            Rp1SpiEvtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE      Rp1SpiEvtPrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE      Rp1SpiEvtReleaseHardware;
EVT_SPB_TARGET_CONNECT               Rp1SpiEvtTargetConnect;
EVT_SPB_TARGET_DISCONNECT            Rp1SpiEvtTargetDisconnect;
EVT_SPB_CONTROLLER_READ              Rp1SpiEvtIoRead;
EVT_SPB_CONTROLLER_WRITE             Rp1SpiEvtIoWrite;
EVT_SPB_CONTROLLER_SEQUENCE          Rp1SpiEvtIoSequence;
