/*++
Module Name: common.h
Abstract:    CYW43455 Wi-Fi (brcmfmac, SDIO) - NDIS miniport REGISTRATION skeleton.

    HONEST SCOPE: a real Windows Wi-Fi driver is a WDI (WLAN Device Driver
    Interface) or Native-802.11 miniport - an enormous framework (scan/connect,
    hundreds of dot11 OIDs) layered on an SDIO function driver that loads the
    CYW43455 firmware. That is far beyond a no-hardware skeleton. This module
    only stands up the NDIS miniport registration so the adapter object exists;
    the 802.11/WDI protocol layer + SDIO transport + firmware are NOT done.
--*/
#pragma once

#define NDIS_MINIPORT_DRIVER   1
#define NDIS630_MINIPORT       1
#define NDIS_SUPPORT_NDIS6     1
#include <ndis.h>

#define WL_TAG  'lWyR'   // 'RyWl'

typedef struct _WL_ADAPTER {
    NDIS_HANDLE MiniportHandle;
    PVOID       SdioFunction;   /* CYW43455 SDIO function (transport) - TODO */
} WL_ADAPTER, *PWL_ADAPTER;

DRIVER_INITIALIZE DriverEntry;

MINIPORT_INITIALIZE               WlInitialize;
MINIPORT_HALT                     WlHalt;
MINIPORT_PAUSE                    WlPause;
MINIPORT_RESTART                  WlRestart;
MINIPORT_OID_REQUEST              WlOidRequest;
MINIPORT_SEND_NET_BUFFER_LISTS    WlSendNetBufferLists;
MINIPORT_RETURN_NET_BUFFER_LISTS  WlReturnNetBufferLists;
MINIPORT_CANCEL_SEND              WlCancelSend;
MINIPORT_CHECK_FOR_HANG           WlCheckForHang;
MINIPORT_RESET                    WlReset;
MINIPORT_DEVICE_PNP_EVENT_NOTIFY  WlDevicePnpEventNotify;
MINIPORT_SHUTDOWN                 WlShutdown;
MINIPORT_CANCEL_OID_REQUEST       WlCancelOidRequest;
