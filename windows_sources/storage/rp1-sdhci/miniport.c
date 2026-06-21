/*++
Module Name: miniport.c
Abstract:    SdPort miniport for the RP1/BCM2712 SDHCI host. DriverEntry registers
             with sdport.sys; the SdPort callbacks drive the SDHCI command engine
             (sdhci_hw.c, verified by the x64 sdhci_sim harness). DMA data path +
             tuning + voltage switch remain for real-hardware bring-up.
--*/
#include "common.h"
#include "sdhci_hw.h"

/* Map SdPort response type -> SDHCI command response encoding. */
static unsigned char Rp1SdRespType(SDPORT_RESPONSE_TYPE Type)
{
    if (Type == SdResponseTypeNone) return SDHCI_CMD_RESP_NONE;
    if (Type == SdResponseTypeR2)   return SDHCI_CMD_RESP_LONG;
    return SDHCI_CMD_RESP_SHORT;    /* R1/R3/R4/R5/R6/R7 (R1b busy = refinement) */
}

NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    SDPORT_INITIALIZATION_DATA init;

    RtlZeroMemory(&init, sizeof(init));
    init.StructureSize        = sizeof(SDPORT_INITIALIZATION_DATA);
    init.GetSlotCount         = Rp1SdGetSlotCount;
    init.GetSlotCapabilities  = Rp1SdGetSlotCapabilities;
    init.Interrupt            = Rp1SdInterrupt;
    init.IssueRequest         = Rp1SdIssueRequest;
    init.GetResponse          = Rp1SdGetResponse;
    init.RequestDpc           = Rp1SdRequestDpc;
    init.ToggleEvents         = Rp1SdToggleEvents;
    init.ClearEvents          = Rp1SdClearEvents;
    init.SaveContext          = Rp1SdSaveContext;
    init.RestoreContext       = Rp1SdRestoreContext;
    init.Initialize           = Rp1SdInitialize;
    init.IssueBusOperation    = Rp1SdIssueBusOperation;
    init.GetCardDetectState   = Rp1SdGetCardDetectState;
    init.GetWriteProtectState = Rp1SdGetWriteProtectState;
    init.Cleanup              = Rp1SdCleanup;
    init.PrivateExtensionSize = sizeof(RP1SD_EXT);
    init.CrashdumpSupported   = FALSE;

    return SdPortInitialize(DriverObject, RegistryPath, &init);
}

NTSTATUS
Rp1SdGetSlotCount(_In_ struct _SD_MINIPORT *Miniport, _Out_ PUCHAR SlotCount)
{
    UNREFERENCED_PARAMETER(Miniport);
    *SlotCount = 1;
    return STATUS_SUCCESS;
}

VOID
Rp1SdGetSlotCapabilities(_In_ PVOID PrivateExtension, _Out_ struct _SDPORT_CAPABILITIES *Capabilities)
{
    UNREFERENCED_PARAMETER(PrivateExtension);
    RtlZeroMemory(Capabilities, sizeof(SDPORT_CAPABILITIES));
    Capabilities->SpecVersion                = 2;
    Capabilities->MaximumOutstandingRequests = 1;
    Capabilities->MaximumBlockSize           = 512;
    Capabilities->MaximumBlockCount          = 0xFFFF;
    /* BCM2712 SDHCI base clock = emmc2-clock = 200 MHz (Pi5-measured via
     * clk_summary; shared by both mmc0 @0x1000fff000 / mmc1 @0x1001100000). */
    Capabilities->BaseClockFrequencyKhz      = 200000;
    Capabilities->Supported.HighSpeed        = 1;
    Capabilities->Supported.Voltage33V       = 1;
}

NTSTATUS
Rp1SdInitialize(_In_ PVOID PrivateExtension, _In_ PHYSICAL_ADDRESS PhysicalBase,
    _In_ PVOID VirtualBase, _In_ ULONG Length, _In_ BOOLEAN CrashdumpMode)
{
    PRP1SD_EXT ext = (PRP1SD_EXT)PrivateExtension;
    UNREFERENCED_PARAMETER(PhysicalBase);
    UNREFERENCED_PARAMETER(CrashdumpMode);
    ext->Base   = VirtualBase;
    ext->Length = Length;

    (void)SdhciSoftReset(VirtualBase, SDHCI_RESET_ALL);
    SdhciSetPower(VirtualBase, SDHCI_POWER_330);
    (void)SdhciSetClock(VirtualBase, 0x40);
    SdhciSetIntEnable(VirtualBase, SDHCI_INT_DEFAULT_MASK);   /* completion + error bits */
    return STATUS_SUCCESS;
}

BOOLEAN
Rp1SdInterrupt(_In_ PVOID PrivateExtension, _Out_ PULONG Events, _Out_ PULONG Errors,
    _Out_ PBOOLEAN NotifyCardChange, _Out_ PBOOLEAN NotifySdioInterrupt, _Out_ PBOOLEAN NotifyTuning)
{
    PRP1SD_EXT   ext = (PRP1SD_EXT)PrivateExtension;
    unsigned int status = SdhciGetIntStatus(ext->Base);

    *Events  = status & 0xFFFF;            /* normal int status */
    *Errors  = (status >> 16) & 0xFFFF;    /* error int status */
    *NotifyCardChange    = (status & (SDHCI_INT_CARD_INSERT | SDHCI_INT_CARD_REMOVE)) ? TRUE : FALSE;
    *NotifySdioInterrupt = FALSE;
    *NotifyTuning        = FALSE;
    if (status != 0) {
        SdhciAckInt(ext->Base, status);    /* write-1-to-clear handled bits */
        return TRUE;
    }
    return FALSE;
}

NTSTATUS
Rp1SdIssueRequest(_In_ PVOID PrivateExtension, _In_ struct _SDPORT_REQUEST *Request)
{
    PRP1SD_EXT      ext = (PRP1SD_EXT)PrivateExtension;
    PSDPORT_COMMAND cmd = &Request->Command;
    unsigned char   rt  = Rp1SdRespType(cmd->ResponseType);
    int             dataPresent = (cmd->TransferType != SdTransferTypeNone);

    unsigned short xferMode = 0;
    if (dataPresent) {
        SdhciSetBlock(ext->Base, cmd->BlockSize, cmd->BlockCount);
        /* TRANSFER_MODE: enable block count, set direction + multi-block. */
        xferMode = SDHCI_TRNS_BLK_CNT_EN;
        if (cmd->TransferDirection == SdTransferDirectionRead) xferMode |= SDHCI_TRNS_READ;
        if (cmd->BlockCount > 1)                                xferMode |= SDHCI_TRNS_MULTI;
    }
    if (SdhciSendCommand(ext->Base, (unsigned char)cmd->Index, cmd->Argument, rt,
                         dataPresent, xferMode) != 0) {
        return STATUS_IO_TIMEOUT;
    }
    /* NOTE: PIO/DMA data movement for data commands is the real-hardware path. */
    return STATUS_SUCCESS;
}

VOID
Rp1SdGetResponse(_In_ PVOID PrivateExtension, _In_ struct _SDPORT_COMMAND *Command,
    _Out_ PVOID ResponseBuffer)
{
    PRP1SD_EXT    ext = (PRP1SD_EXT)PrivateExtension;
    unsigned char rt  = Rp1SdRespType(Command->ResponseType);
    unsigned int  resp[4];

    SdhciReadResponse(ext->Base, rt, resp);
    if (ResponseBuffer != NULL) {
        RtlCopyMemory(ResponseBuffer, resp, (rt == SDHCI_CMD_RESP_LONG) ? 16 : 4);
    }
}

VOID
Rp1SdRequestDpc(_In_ PVOID PrivateExtension, _Inout_ struct _SDPORT_REQUEST *Request,
    _In_ ULONG Events, _In_ ULONG Errors)
{
    UNREFERENCED_PARAMETER(PrivateExtension);
    UNREFERENCED_PARAMETER(Events);
    Request->Status = (Errors != 0) ? STATUS_IO_DEVICE_ERROR : STATUS_SUCCESS;
}

VOID
Rp1SdToggleEvents(_In_ PVOID PrivateExtension, _In_ ULONG EventMask, _In_ BOOLEAN Enable)
{
    PRP1SD_EXT ext = (PRP1SD_EXT)PrivateExtension;
    UNREFERENCED_PARAMETER(EventMask);
    if (Enable) {
        SdhciSetIntEnable(ext->Base, SDHCI_INT_DEFAULT_MASK);   /* incl. all error bits */
    } else {
        SdhciSetIntEnable(ext->Base, 0);
    }
}

VOID
Rp1SdClearEvents(_In_ PVOID PrivateExtension, _In_ ULONG EventMask)
{
    PRP1SD_EXT ext = (PRP1SD_EXT)PrivateExtension;
    SdhciAckInt(ext->Base, EventMask);
}

VOID Rp1SdSaveContext(_In_ PVOID PrivateExtension)    { UNREFERENCED_PARAMETER(PrivateExtension); }
VOID Rp1SdRestoreContext(_In_ PVOID PrivateExtension) { UNREFERENCED_PARAMETER(PrivateExtension); }

NTSTATUS
Rp1SdIssueBusOperation(_In_ PVOID PrivateExtension, _In_ struct _SDPORT_BUS_OPERATION *BusOperation)
{
    PRP1SD_EXT ext = (PRP1SD_EXT)PrivateExtension;
    switch (BusOperation->Type) {
    case SdResetHw:
        (void)SdhciSoftReset(ext->Base, SDHCI_RESET_ALL);
        break;
    case SdSetVoltage:
        switch (BusOperation->Parameters.Voltage) {
        case SdBusVoltage33: SdhciSetPower(ext->Base, SDHCI_POWER_330); break;
        case SdBusVoltage30: SdhciSetPower(ext->Base, SDHCI_POWER_300); break;
        case SdBusVoltage18: SdhciSetPower(ext->Base, SDHCI_POWER_180); break;
        default: break;
        }
        break;
    case SdSetBusWidth:
        SdhciSetBusWidth(ext->Base, BusOperation->Parameters.BusWidth >= SdBusWidth4Bit);
        break;
    case SdSetBusSpeed:
        SdhciSetHighSpeed(ext->Base, BusOperation->Parameters.BusSpeed >= SdBusSpeedHigh);
        break;
    default:
        /* SdSetClock divider tuning + signaling-voltage switch are hardware-path. */
        break;
    }
    return STATUS_SUCCESS;
}

BOOLEAN
Rp1SdGetCardDetectState(_In_ PVOID PrivateExtension)
{
    PRP1SD_EXT ext = (PRP1SD_EXT)PrivateExtension;
    return SdhciCardPresent(ext->Base) ? TRUE : FALSE;
}

BOOLEAN
Rp1SdGetWriteProtectState(_In_ PVOID PrivateExtension)
{
    UNREFERENCED_PARAMETER(PrivateExtension);
    return FALSE;
}

VOID
Rp1SdCleanup(_In_ struct _SD_MINIPORT *Miniport)
{
    UNREFERENCED_PARAMETER(Miniport);
}
