/*++
Module Name: sdhci_hw.c
Abstract:    SDHCI command-engine HAL (shared by the kernel driver and the x64
             simulation harness). Ported from the SDHCI spec / Linux sdhci.c.
--*/
#include "sdhci_regio.h"
#include "sdhci_hw.h"

#define SDHCI_POLL_MAX  100000   /* bounded spin (driver replaces with stall) */

/* Issue a software reset for the given block(s) and wait for self-clear. */
int SdhciSoftReset(void *Base, unsigned char Mask)
{
    unsigned i;
    WR8(Base, SDHCI_SOFTWARE_RESET, Mask);
    for (i = 0; i < SDHCI_POLL_MAX; i++) {
        if ((RD8(Base, SDHCI_SOFTWARE_RESET) & Mask) == 0) {
            return 0;
        }
    }
    return -1;
}

/* Program the internal clock divisor, wait for stable, enable the card clock. */
int SdhciSetClock(void *Base, unsigned short DivSelect)
{
    unsigned short clk;
    unsigned i;

    WR16(Base, SDHCI_CLOCK_CONTROL, 0);                 /* stop clock */
    clk = (unsigned short)((DivSelect << 8) | SDHCI_CLOCK_INT_EN);
    WR16(Base, SDHCI_CLOCK_CONTROL, clk);
    for (i = 0; i < SDHCI_POLL_MAX; i++) {
        if (RD16(Base, SDHCI_CLOCK_CONTROL) & SDHCI_CLOCK_INT_STABLE) {
            break;
        }
    }
    if (i == SDHCI_POLL_MAX) {
        return -1;
    }
    clk = (unsigned short)(RD16(Base, SDHCI_CLOCK_CONTROL) | SDHCI_CLOCK_CARD_EN);
    WR16(Base, SDHCI_CLOCK_CONTROL, clk);
    return 0;
}

void SdhciSetPower(void *Base, unsigned char VoltageBits)
{
    /* sdhci_set_power_noreg: drop power, then apply the new voltage + power-on. */
    WR8(Base, SDHCI_POWER_CONTROL, 0);
    WR8(Base, SDHCI_POWER_CONTROL, (unsigned char)(VoltageBits | SDHCI_POWER_ON));
}

/* HOST_CONTROL bus width (read-modify-write the 4-bit-bus field). */
void SdhciSetBusWidth(void *Base, int FourBit)
{
    unsigned char ctrl = RD8(Base, SDHCI_HOST_CONTROL);
    if (FourBit) ctrl |= SDHCI_CTRL_4BITBUS;
    else         ctrl = (unsigned char)(ctrl & ~SDHCI_CTRL_4BITBUS);
    WR8(Base, SDHCI_HOST_CONTROL, ctrl);
}

/* HOST_CONTROL high-speed enable. */
void SdhciSetHighSpeed(void *Base, int Enable)
{
    unsigned char ctrl = RD8(Base, SDHCI_HOST_CONTROL);
    if (Enable) ctrl |= SDHCI_CTRL_HISPD;
    else        ctrl = (unsigned char)(ctrl & ~SDHCI_CTRL_HISPD);
    WR8(Base, SDHCI_HOST_CONTROL, ctrl);
}

void SdhciSetBlock(void *Base, unsigned short BlockSize, unsigned short BlockCount)
{
    /* sdhci_set_block_info: encode the SDMA buffer boundary in BLOCK_SIZE[14:12]. */
    WR16(Base, SDHCI_BLOCK_SIZE, SDHCI_MAKE_BLKSZ(SDHCI_DEFAULT_BOUNDARY, BlockSize));
    WR16(Base, SDHCI_BLOCK_COUNT, BlockCount);
}

/*
 * Program Argument + Command. Waits for CMD (and DAT, if data) inhibit to clear
 * before issuing, per the SDHCI command sequence.
 */
int SdhciSendCommand(void *Base, unsigned char CmdIndex, unsigned int Arg,
                     unsigned char RespType, int DataPresent, unsigned short TransferMode)
{
    unsigned int   mask = SDHCI_STATE_CMD_INHIBIT | (DataPresent ? SDHCI_STATE_DAT_INHIBIT : 0);
    unsigned short flags = RespType & 0x03;
    unsigned       i;

    for (i = 0; i < SDHCI_POLL_MAX; i++) {
        if ((RD32(Base, SDHCI_PRESENT_STATE) & mask) == 0) {
            break;
        }
    }
    if (i == SDHCI_POLL_MAX) {
        return -1;
    }

    /* CRC + index check on for responses that carry them (short/long, not R3). */
    if (RespType == SDHCI_CMD_RESP_LONG) {
        flags |= SDHCI_CMD_CRC;
    } else if (RespType == SDHCI_CMD_RESP_SHORT || RespType == SDHCI_CMD_RESP_SHORT_BUSY) {
        flags |= SDHCI_CMD_CRC | SDHCI_CMD_INDEX;
    }
    if (DataPresent) {
        flags |= SDHCI_CMD_DATA;
        /* Data commands need a data-timeout (else immediate Data Timeout error) and
         * a TRANSFER_MODE telling the engine direction / block count (sdhci_set_timeout
         * + sdhci_set_transfer_mode). Both were previously never written. */
        WR8(Base, SDHCI_TIMEOUT_CONTROL, SDHCI_TIMEOUT_MAX);
        WR16(Base, SDHCI_TRANSFER_MODE, TransferMode);
    }

    WR32(Base, SDHCI_ARGUMENT, Arg);
    WR16(Base, SDHCI_COMMAND, SDHCI_MAKE_CMD(CmdIndex, flags));
    return 0;
}

void SdhciReadResponse(void *Base, unsigned char RespType, unsigned int Resp[4])
{
    if (RespType == SDHCI_CMD_RESP_LONG) {
        Resp[0] = RD32(Base, SDHCI_RESPONSE + 0);
        Resp[1] = RD32(Base, SDHCI_RESPONSE + 4);
        Resp[2] = RD32(Base, SDHCI_RESPONSE + 8);
        Resp[3] = RD32(Base, SDHCI_RESPONSE + 12);
    } else if (RespType != SDHCI_CMD_RESP_NONE) {
        Resp[0] = RD32(Base, SDHCI_RESPONSE + 0);
        Resp[1] = Resp[2] = Resp[3] = 0;
    } else {
        Resp[0] = Resp[1] = Resp[2] = Resp[3] = 0;
    }
}

unsigned int SdhciGetIntStatus(void *Base)
{
    return RD32(Base, SDHCI_INT_STATUS);
}

void SdhciAckInt(void *Base, unsigned int Mask)
{
    WR32(Base, SDHCI_INT_STATUS, Mask);   /* write-1-to-clear */
}

void SdhciSetIntEnable(void *Base, unsigned int Mask)
{
    WR32(Base, SDHCI_INT_ENABLE, Mask);
    WR32(Base, SDHCI_SIGNAL_ENABLE, Mask);
}

int SdhciCardPresent(void *Base)
{
    return (RD32(Base, SDHCI_PRESENT_STATE) & SDHCI_STATE_CARD_PRESENT) ? 1 : 0;
}
