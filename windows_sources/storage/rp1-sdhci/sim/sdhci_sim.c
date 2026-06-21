/*++
Module Name: sdhci_sim.c
Abstract:    x64 user-mode simulation harness for the SDHCI command engine
             (sdhci_hw.c). Provides a mock register space with enough device
             behaviour for the HAL's bounded polls to terminate, then runs a
             command sequence and asserts the engine programmed the right
             registers. Proves the register logic without real hardware.

             Build (x64): cl /DSDHCI_SIM /I.. sdhci_sim.c ..\sdhci_hw.c
--*/
#include <stdio.h>
#include <string.h>
#include "../sdhci_hw.h"

/* ---- mock register space (little-endian byte array) ---- */
static unsigned char g_regs[0x100];

static unsigned int load(unsigned off, int width)
{
    unsigned int v = 0;
    int i;
    for (i = 0; i < width; i++) v |= ((unsigned int)g_regs[off + i]) << (8 * i);
    return v;
}
static void store(unsigned off, int width, unsigned int val)
{
    int i;
    for (i = 0; i < width; i++) g_regs[off + i] = (unsigned char)((val >> (8 * i)) & 0xFF);
}

unsigned int SimRd(void *base, unsigned off, int width)
{
    (void)base;
    return load(off, width);
}

void SimWr(void *base, unsigned off, int width, unsigned val)
{
    (void)base;
    store(off, width, val);

    /* ---- model device side-effects so the HAL polls converge ---- */
    if (off == SDHCI_SOFTWARE_RESET) {
        store(SDHCI_SOFTWARE_RESET, 1, 0);                 /* reset self-clears */
    }
    if (off == SDHCI_CLOCK_CONTROL && (val & SDHCI_CLOCK_INT_EN)) {
        store(SDHCI_CLOCK_CONTROL, 2, val | SDHCI_CLOCK_INT_STABLE); /* clock stable */
    }
    if (off == SDHCI_COMMAND) {
        store(SDHCI_INT_STATUS, 4, load(SDHCI_INT_STATUS, 4) | SDHCI_INT_CMD_COMPLETE);
    }
    if (off == SDHCI_INT_STATUS) {
        store(SDHCI_INT_STATUS, 4, load(SDHCI_INT_STATUS, 4) & ~val); /* W1C */
    }
}

/* ---- test harness ---- */
static int g_pass, g_fail;
static void check(const char *what, int cond)
{
    if (cond) { g_pass++; printf("  [PASS] %s\n", what); }
    else      { g_fail++; printf("  [FAIL] %s\n", what); }
}

int main(void)
{
    void *base = g_regs;          /* base is opaque to the HAL */
    unsigned int resp[4];
    int rc;

    memset(g_regs, 0, sizeof(g_regs));
    /* card inserted, inhibits clear */
    store(SDHCI_PRESENT_STATE, 4, SDHCI_STATE_CARD_PRESENT);
    /* preload a response value (e.g. CMD8 echo / R1) */
    store(SDHCI_RESPONSE, 4, 0x12345678u);

    printf("== SDHCI command-engine simulation ==\n");

    rc = SdhciSoftReset(base, SDHCI_RESET_ALL);
    check("SoftReset(ALL) returns success", rc == 0);
    check("SoftReset bit self-cleared", (g_regs[SDHCI_SOFTWARE_RESET] & SDHCI_RESET_ALL) == 0);

    rc = SdhciSetClock(base, 0x40);
    check("SetClock returns success", rc == 0);
    check("card clock enabled", (load(SDHCI_CLOCK_CONTROL, 2) & SDHCI_CLOCK_CARD_EN) != 0);

    check("CardPresent reports inserted", SdhciCardPresent(base) == 1);

    /* CMD0 (GO_IDLE): no response */
    rc = SdhciSendCommand(base, 0, 0, SDHCI_CMD_RESP_NONE, 0, 0);
    check("CMD0 issued", rc == 0);
    check("CMD0 argument == 0", load(SDHCI_ARGUMENT, 4) == 0);
    check("CMD0 command index == 0", SDHCI_CMD_GET_INDEX(load(SDHCI_COMMAND, 2)) == 0);

    /* CMD8 (SEND_IF_COND): arg 0x1AA, short response */
    rc = SdhciSendCommand(base, 8, 0x1AA, SDHCI_CMD_RESP_SHORT, 0, 0);
    check("CMD8 issued", rc == 0);
    check("CMD8 argument == 0x1AA", load(SDHCI_ARGUMENT, 4) == 0x1AA);
    check("CMD8 command index == 8", SDHCI_CMD_GET_INDEX(load(SDHCI_COMMAND, 2)) == 8);
    check("CMD8 has CRC+INDEX check", (load(SDHCI_COMMAND, 2) & (SDHCI_CMD_CRC | SDHCI_CMD_INDEX))
                                       == (SDHCI_CMD_CRC | SDHCI_CMD_INDEX));

    check("INT status shows CMD_COMPLETE", (SdhciGetIntStatus(base) & SDHCI_INT_CMD_COMPLETE) != 0);

    SdhciReadResponse(base, SDHCI_CMD_RESP_SHORT, resp);
    check("response[0] read == 0x12345678", resp[0] == 0x12345678u);

    SdhciAckInt(base, SDHCI_INT_CMD_COMPLETE);
    check("CMD_COMPLETE acked (W1C)", (SdhciGetIntStatus(base) & SDHCI_INT_CMD_COMPLETE) == 0);

    /* default IRQ mask must enable the error bits, else errors never latch */
    SdhciSetIntEnable(base, SDHCI_INT_DEFAULT_MASK);
    check("INT enable has CMD error bits", (load(SDHCI_INT_ENABLE, 4) & SDHCI_INT_CMD_ERRORS) == SDHCI_INT_CMD_ERRORS);
    check("INT enable has DATA error bits", (load(SDHCI_INT_ENABLE, 4) & SDHCI_INT_DATA_ERRORS) == SDHCI_INT_DATA_ERRORS);
    check("SIGNAL enable mirrors INT enable", load(SDHCI_SIGNAL_ENABLE, 4) == load(SDHCI_INT_ENABLE, 4));

    /* block transfer setup: size in [11:0], SDMA boundary in [14:12] */
    SdhciSetBlock(base, 512, 8);
    check("block size field == 512", (load(SDHCI_BLOCK_SIZE, 2) & 0xFFF) == 512);
    check("block size SDMA boundary == 7 (512K)", ((load(SDHCI_BLOCK_SIZE, 2) >> 12) & 0x7) == SDHCI_DEFAULT_BOUNDARY);
    check("block count == 8", load(SDHCI_BLOCK_COUNT, 2) == 8);

    /* data command (CMD17 read): TRANSFER_MODE + TIMEOUT must be programmed */
    rc = SdhciSendCommand(base, 17, 0, SDHCI_CMD_RESP_SHORT, 1, SDHCI_TRNS_BLK_CNT_EN | SDHCI_TRNS_READ);
    check("CMD17 (data) issued", rc == 0);
    check("TRANSFER_MODE = BLK_CNT_EN|READ", load(SDHCI_TRANSFER_MODE, 2) == (SDHCI_TRNS_BLK_CNT_EN | SDHCI_TRNS_READ));
    check("TIMEOUT_CONTROL = max", load(SDHCI_TIMEOUT_CONTROL, 1) == SDHCI_TIMEOUT_MAX);
    check("CMD17 has DATA flag", (load(SDHCI_COMMAND, 2) & SDHCI_CMD_DATA) != 0);

    /* host control bus width / high speed */
    SdhciSetBusWidth(base, 1);
    check("HOST_CONTROL 4-bit bus set", (load(SDHCI_HOST_CONTROL, 1) & SDHCI_CTRL_4BITBUS) != 0);
    SdhciSetHighSpeed(base, 1);
    check("HOST_CONTROL high-speed set", (load(SDHCI_HOST_CONTROL, 1) & SDHCI_CTRL_HISPD) != 0);
    check("HOST_CONTROL preserves 4-bit when setting HS", (load(SDHCI_HOST_CONTROL, 1) & SDHCI_CTRL_4BITBUS) != 0);
    SdhciSetBusWidth(base, 0);
    check("HOST_CONTROL 4-bit cleared", (load(SDHCI_HOST_CONTROL, 1) & SDHCI_CTRL_4BITBUS) == 0);

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
