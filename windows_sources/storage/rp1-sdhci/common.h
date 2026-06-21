/*++
Module Name: common.h
Abstract:    RP1 / BCM2712 SD/MMC host controller miniport (SdPort).
             Registers with the inbox sdport.sys storage port driver. The RP1 SD
             host is a DesignWare MSHC (SDHCI-compatible). This skeleton stands
             up the miniport DDI; the SDHCI command engine is the next stage.
--*/
#pragma once

#include <ntddk.h>
#include <sdport.h>

#define RP1SD_POOLTAG  'dSpR'   // 'RpSd'

// Per-slot private extension (sized via SDPORT_INITIALIZATION_DATA.PrivateExtensionSize).
typedef struct _RP1SD_EXT {
    PVOID Base;     // SDHCI register base (from SDPORT_INITIALIZE.VirtualBase)
    ULONG Length;
} RP1SD_EXT, *PRP1SD_EXT;

// sdport miniport callbacks (signatures pinned by the sdport.h function typedefs)
SDPORT_GET_SLOT_COUNT          Rp1SdGetSlotCount;
SDPORT_GET_SLOT_CAPABILITIES   Rp1SdGetSlotCapabilities;
SDPORT_INTERRUPT               Rp1SdInterrupt;
SDPORT_ISSUE_REQUEST           Rp1SdIssueRequest;
SDPORT_GET_RESPONSE            Rp1SdGetResponse;
SDPORT_REQUEST_DPC             Rp1SdRequestDpc;
SDPORT_TOGGLE_EVENTS           Rp1SdToggleEvents;
SDPORT_CLEAR_EVENTS            Rp1SdClearEvents;
SDPORT_SAVE_CONTEXT            Rp1SdSaveContext;
SDPORT_RESTORE_CONTEXT         Rp1SdRestoreContext;
SDPORT_INITIALIZE              Rp1SdInitialize;
SDPORT_ISSUE_BUS_OPERATION     Rp1SdIssueBusOperation;
SDPORT_GET_CARD_DETECT_STATE   Rp1SdGetCardDetectState;
SDPORT_GET_WRITE_PROTECT_STATE Rp1SdGetWriteProtectState;
SDPORT_CLEANUP                 Rp1SdCleanup;
