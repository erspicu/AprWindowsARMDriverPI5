/*++
Module Name: common.h
Abstract:    BCM2712 SPI controller driver (SpbCx) - shared decls.
             Same SpbCx pattern as the RP1 SPI driver, with the BCM SPI HAL.
--*/
#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <spb.h>
#include <spbcx.h>
#include "bcm_spi_hw.h"

#define BCMSPI_POOLTAG  'pScB'   // 'BcSp'

typedef struct _BCMSPI_DEVICE {
    BCM_SPI_HW Hw;
    PUCHAR     Mmio;
    SIZE_T     MmioLen;
} BCMSPI_DEVICE, *PBCMSPI_DEVICE;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BCMSPI_DEVICE, BcmSpiGetContext)

EVT_WDF_DRIVER_DEVICE_ADD            BcmSpiEvtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE      BcmSpiEvtPrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE      BcmSpiEvtReleaseHardware;
EVT_SPB_TARGET_CONNECT               BcmSpiEvtTargetConnect;
EVT_SPB_TARGET_DISCONNECT            BcmSpiEvtTargetDisconnect;
EVT_SPB_CONTROLLER_READ              BcmSpiEvtIoRead;
EVT_SPB_CONTROLLER_WRITE             BcmSpiEvtIoWrite;
EVT_SPB_CONTROLLER_SEQUENCE          BcmSpiEvtIoSequence;
