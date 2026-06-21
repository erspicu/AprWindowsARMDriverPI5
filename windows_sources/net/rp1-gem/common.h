/*++
Module Name: common.h
Abstract:    RP1 Gigabit Ethernet (Cadence GEM / macb) NDIS miniport.
             Registers an NDIS 6.30 miniport with the network stack. This
             skeleton stands up the miniport DDI + adapter registration; the
             GEM DMA ring TX/RX engine + PHY is the next stage (needs HW).
--*/
#pragma once

#define NDIS_MINIPORT_DRIVER   1
#define NDIS630_MINIPORT       1
#define NDIS_SUPPORT_NDIS6     1
#include <ndis.h>

#define GEM_TAG  'meGR'   // 'RGem'

typedef struct _GEM_ADAPTER {
    NDIS_HANDLE MiniportHandle;
    PVOID       Mmio;
    SIZE_T      MmioLen;
} GEM_ADAPTER, *PGEM_ADAPTER;

DRIVER_INITIALIZE DriverEntry;

// NDIS miniport handlers (signatures pinned by the ndis.h function typedefs)
MINIPORT_INITIALIZE               GemInitialize;
MINIPORT_HALT                     GemHalt;
MINIPORT_PAUSE                    GemPause;
MINIPORT_RESTART                  GemRestart;
MINIPORT_OID_REQUEST              GemOidRequest;
MINIPORT_SEND_NET_BUFFER_LISTS    GemSendNetBufferLists;
MINIPORT_RETURN_NET_BUFFER_LISTS  GemReturnNetBufferLists;
MINIPORT_CANCEL_SEND              GemCancelSend;
MINIPORT_CHECK_FOR_HANG           GemCheckForHang;
MINIPORT_RESET                    GemReset;
MINIPORT_DEVICE_PNP_EVENT_NOTIFY  GemDevicePnpEventNotify;
MINIPORT_SHUTDOWN                 GemShutdown;
MINIPORT_CANCEL_OID_REQUEST       GemCancelOidRequest;
