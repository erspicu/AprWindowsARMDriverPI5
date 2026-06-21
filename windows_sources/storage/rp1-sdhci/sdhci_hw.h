/*++
Module Name: sdhci_hw.h
Abstract:    Standard SD Host Controller (SDHCI) register map + command-engine
             HAL. Ported from the SDHCI spec (matches Linux drivers/mmc/host/
             sdhci.c register usage). Pure register logic, no SdPort/OS types,
             so it is verifiable by the x64 simulation harness.
--*/
#pragma once

/* ---- SDHCI register offsets ---- */
#define SDHCI_DMA_ADDRESS      0x00
#define SDHCI_BLOCK_SIZE       0x04   /* 16-bit */
#define SDHCI_BLOCK_COUNT      0x06   /* 16-bit */
#define SDHCI_ARGUMENT         0x08   /* 32-bit */
#define SDHCI_TRANSFER_MODE    0x0C   /* 16-bit */
#define SDHCI_COMMAND          0x0E   /* 16-bit */
#define SDHCI_RESPONSE         0x10   /* 4 x 32-bit */
#define SDHCI_BUFFER           0x20   /* 32-bit PIO data port */
#define SDHCI_PRESENT_STATE    0x24   /* 32-bit */
#define SDHCI_HOST_CONTROL     0x28   /* 8-bit */
#define SDHCI_POWER_CONTROL    0x29   /* 8-bit */
#define SDHCI_CLOCK_CONTROL    0x2C   /* 16-bit */
#define SDHCI_TIMEOUT_CONTROL  0x2E   /* 8-bit */
#define SDHCI_SOFTWARE_RESET   0x2F   /* 8-bit */
#define SDHCI_INT_STATUS       0x30   /* normal(16) | error(16) -> 32-bit */
#define SDHCI_INT_ENABLE       0x34   /* status enable */
#define SDHCI_SIGNAL_ENABLE    0x38   /* signal (IRQ) enable */
#define SDHCI_CAPABILITIES     0x40   /* 32-bit */

/* ---- Command register (0x0E) ---- */
#define SDHCI_CMD_RESP_NONE        0x00
#define SDHCI_CMD_RESP_LONG        0x01   /* R2  (136-bit) */
#define SDHCI_CMD_RESP_SHORT       0x02   /* R1/R3/R6/R7 (48-bit) */
#define SDHCI_CMD_RESP_SHORT_BUSY  0x03   /* R1b (48-bit + busy) */
#define SDHCI_CMD_CRC              0x08   /* CRC check enable */
#define SDHCI_CMD_INDEX            0x10   /* index check enable */
#define SDHCI_CMD_DATA             0x20   /* data present */
#define SDHCI_MAKE_CMD(idx,flags)  ((unsigned short)((((idx) & 0x3F) << 8) | ((flags) & 0xFF)))
#define SDHCI_CMD_GET_INDEX(reg)   (((reg) >> 8) & 0x3F)

/* ---- Block Size (0x04): SDMA buffer boundary in [14:12], size in [11:0] ---- */
#define SDHCI_DEFAULT_BOUNDARY     7            /* 512K (SDHCI_DEFAULT_BOUNDARY_ARG) */
#define SDHCI_MAKE_BLKSZ(b,sz)     ((unsigned short)((((b) & 0x7) << 12) | ((sz) & 0xFFF)))

/* ---- Transfer Mode (0x0C) ---- */
#define SDHCI_TRNS_DMA             0x0001
#define SDHCI_TRNS_BLK_CNT_EN      0x0002
#define SDHCI_TRNS_READ            0x0010
#define SDHCI_TRNS_MULTI           0x0020

/* ---- Host Control (0x28) ---- */
#define SDHCI_CTRL_4BITBUS         0x02
#define SDHCI_CTRL_HISPD           0x04

/* ---- Timeout Control (0x2E): data timeout counter; 0x0E = max (TMCLK<<27) ---- */
#define SDHCI_TIMEOUT_MAX          0x0E

/* ---- Present State (0x24) ---- */
#define SDHCI_STATE_CMD_INHIBIT    0x00000001
#define SDHCI_STATE_DAT_INHIBIT    0x00000002
#define SDHCI_STATE_CARD_PRESENT   0x00010000

/* ---- Normal/Error Int Status (0x30): normal in [15:0], error in [31:16] ---- */
#define SDHCI_INT_CMD_COMPLETE     0x00000001
#define SDHCI_INT_XFER_COMPLETE    0x00000002
#define SDHCI_INT_DMA              0x00000008
#define SDHCI_INT_BUF_WR_READY     0x00000010
#define SDHCI_INT_BUF_RD_READY     0x00000020
#define SDHCI_INT_CARD_INSERT      0x00000040
#define SDHCI_INT_CARD_REMOVE      0x00000080
#define SDHCI_INT_ERROR            0x00008000   /* any error (error status in high 16) */
#define SDHCI_INT_TIMEOUT          0x00010000
#define SDHCI_INT_CRC              0x00020000
#define SDHCI_INT_END_BIT          0x00040000
#define SDHCI_INT_INDEX            0x00080000
#define SDHCI_INT_DATA_TIMEOUT     0x00100000
#define SDHCI_INT_DATA_CRC         0x00200000
#define SDHCI_INT_DATA_END_BIT     0x00400000
#define SDHCI_INT_BUS_POWER        0x00800000
/* Default IRQ mask (sdhci_set_default_irqs): completion + buffer-ready + all the
 * command/data error bits, so errors actually latch and get reported. */
#define SDHCI_INT_CMD_ERRORS  (SDHCI_INT_TIMEOUT | SDHCI_INT_CRC | SDHCI_INT_END_BIT | SDHCI_INT_INDEX)
#define SDHCI_INT_DATA_ERRORS (SDHCI_INT_DATA_TIMEOUT | SDHCI_INT_DATA_CRC | SDHCI_INT_DATA_END_BIT)
#define SDHCI_INT_DEFAULT_MASK \
    (SDHCI_INT_CMD_COMPLETE | SDHCI_INT_XFER_COMPLETE | SDHCI_INT_BUF_WR_READY | \
     SDHCI_INT_BUF_RD_READY | SDHCI_INT_CARD_INSERT | SDHCI_INT_CARD_REMOVE | \
     SDHCI_INT_CMD_ERRORS | SDHCI_INT_DATA_ERRORS | SDHCI_INT_BUS_POWER)

/* ---- Software Reset (0x2F) ---- */
#define SDHCI_RESET_ALL  0x01
#define SDHCI_RESET_CMD  0x02
#define SDHCI_RESET_DATA 0x04

/* ---- Clock Control (0x2C) ---- */
#define SDHCI_CLOCK_INT_EN      0x0001
#define SDHCI_CLOCK_INT_STABLE  0x0002
#define SDHCI_CLOCK_CARD_EN     0x0004

/* ---- Power Control (0x29) ---- */
#define SDHCI_POWER_ON    0x01
#define SDHCI_POWER_180   0x0A
#define SDHCI_POWER_300   0x0C
#define SDHCI_POWER_330   0x0E

/* ---- HAL (returns 0 on success, -1 on timeout/error) ---- */
int  SdhciSoftReset(void *Base, unsigned char Mask);
int  SdhciSetClock(void *Base, unsigned short DivSelect);
void SdhciSetPower(void *Base, unsigned char VoltageBits);
void SdhciSetBusWidth(void *Base, int FourBit);     /* HOST_CONTROL 4BITBUS */
void SdhciSetHighSpeed(void *Base, int Enable);      /* HOST_CONTROL HISPD */
void SdhciSetBlock(void *Base, unsigned short BlockSize, unsigned short BlockCount);
/* TransferMode = OR of SDHCI_TRNS_* (BLK_CNT_EN|READ|MULTI|DMA); ignored when
 * DataPresent == 0. Data commands also get a max data-timeout programmed. */
int  SdhciSendCommand(void *Base, unsigned char CmdIndex, unsigned int Arg,
                      unsigned char RespType, int DataPresent, unsigned short TransferMode);
void SdhciReadResponse(void *Base, unsigned char RespType, unsigned int Resp[4]);
unsigned int SdhciGetIntStatus(void *Base);
void SdhciAckInt(void *Base, unsigned int Mask);
void SdhciSetIntEnable(void *Base, unsigned int Mask);
int  SdhciCardPresent(void *Base);
