/*++
Module Name: sdbus_glue.h
Abstract:    Wires the CYW43455 SDIO control plane (sdio_core.h SDIO_OPS) to the
             Windows inbox SD bus (sdbus.sys) via the SDBUS_INTERFACE_STANDARD /
             SdBusSubmitRequest interface in <ntddsd.h> + <sddef.h>. This is the
             WiFi analog of the BT uart.c glue: it provides the real transport
             behind SDIO_OPS.Cmd52/Cmd53 so Cyw43455ReadChipId() (and WHD) run on
             hardware. Kernel-only; the live path needs the Win11/ARM64 target.
--*/
#pragma once
#include <ntddk.h>
#include <wdf.h>
#include <ntddsd.h>          /* SDBUS_INTERFACE_STANDARD / SDBUS_REQUEST_PACKET / SdBusSubmitRequest */
#include <sddef.h>           /* SDCMD_DESCRIPTOR, SDCMD_IO_RW_DIRECT/EXTENDED, SDTD_*, SDRT_5 */
#include "../sdio_core.h"

typedef struct _WIFI_SDIO {
    SDBUS_INTERFACE_STANDARD SdBus;            /* queried from sdbus.sys (PDO) */
    PVOID                    InterfaceContext; /* = SdBus.Context, for SdBusSubmitRequest */
    SDIO_OPS                 Ops;              /* Cmd52/Cmd53 wired to SdBus */
} WIFI_SDIO, *PWIFI_SDIO;

/* query GUID_SDBUS_INTERFACE_STANDARD from the SD bus and wire SDIO_OPS */
NTSTATUS WifiSdioInit(_Out_ PWIFI_SDIO Sd, _In_ WDFDEVICE Device);

/* convenience: the first bring-up milestone over the real bus (expect 0x4345) */
NTSTATUS WifiSdioReadChipId(_In_ PWIFI_SDIO Sd, _Out_ USHORT *ChipId);
