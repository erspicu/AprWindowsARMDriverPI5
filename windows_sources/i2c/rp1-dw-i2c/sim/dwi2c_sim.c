/*++
Module Name: dwi2c_sim.c
Abstract:    x64 user-mode simulation of the DesignWare I2C HAL (dw_i2c_hw.c).
             Mocks the DW registers + master FIFO behaviour so the polled
             transfer state machine can be exercised and asserted without HW.
             Build: cl /DDWI2C_SIM /I.. dwi2c_sim.c ..\dw_i2c_hw.c
--*/
#include <stdio.h>
#include "../dw_i2c_hw.h"

/* ---- mock register backend ---- */
unsigned char *g_dwBase;                 /* referenced by the shim macros */
static unsigned char g_regs[0x100];      /* generic register storage */
static unsigned g_txLog[64]; static int g_txN;       /* DATA_CMD writes */
static unsigned char g_rxq[64]; static int g_rxHead, g_rxTail;
static const unsigned char g_rxSrc[] = { 0x11, 0x22, 0x33, 0x44 };
static int g_rxSrcN;

static unsigned load(unsigned off)        { unsigned v=0; int i; for(i=0;i<4;i++) v|=((unsigned)g_regs[off+i])<<(8*i); return v; }
static void     store(unsigned off,unsigned v){ int i; for(i=0;i<4;i++) g_regs[off+i]=(unsigned char)((v>>(8*i))&0xFF); }

unsigned int DwSimRd(unsigned off)
{
    switch (off) {
    case DW_IC_COMP_TYPE:     return DW_IC_COMP_TYPE_VALUE;
    case DW_IC_COMP_PARAM_1:  return 0x000F0F00u;        /* tx=16, rx=16 */
    case DW_IC_STATUS: {
        unsigned s = DW_IC_STATUS_TFNF | DW_IC_STATUS_TFE;
        if (g_rxHead < g_rxTail) s |= DW_IC_STATUS_RFNE;
        return s;
    }
    case DW_IC_RAW_INTR_STAT: return 0;                  /* no TX_ABRT */
    case DW_IC_DATA_CMD:      return (g_rxHead < g_rxTail) ? g_rxq[g_rxHead++] : 0;
    default:                  return load(off);
    }
}

void DwSimWr(unsigned off, unsigned val)
{
    if (off == DW_IC_DATA_CMD) {
        if (g_txN < 64) g_txLog[g_txN++] = val;
        if (val & DW_IC_DATA_CMD_READ) {                 /* read request -> enqueue rx byte */
            g_rxq[g_rxTail++] = g_rxSrc[g_rxSrcN++ % (int)sizeof(g_rxSrc)];
        }
        return;
    }
    store(off, val);
}

/* ---- harness ---- */
static int g_pass, g_fail;
static void check(const char *what, int cond)
{
    if (cond) { g_pass++; printf("  [PASS] %s\n", what); }
    else      { g_fail++; printf("  [FAIL] %s\n", what); }
}

int main(void)
{
    DW_I2C_HW hw;
    NTSTATUS  st;
    unsigned char wbuf[2] = { 0xAB, 0xCD };
    unsigned char rbuf[2] = { 0, 0 };

    memset(g_regs, 0, sizeof(g_regs));
    g_dwBase = g_regs;
    printf("== DesignWare I2C transfer-engine simulation ==\n");

    DwI2cHwInit(&hw, g_regs, sizeof(g_regs));
    st = DwI2cHwProbe(&hw);
    check("Probe succeeds (COMP_TYPE matches)", NT_SUCCESS(st));
    check("TX FIFO depth == 16", hw.TxFifoDepth == 16);
    check("RX FIFO depth == 16", hw.RxFifoDepth == 16);

    DwI2cHwConfigureMaster(&hw, DwI2cFast);
    check("MasterCfg = MASTER|SLAVE_DIS|RESTART|FAST",
          hw.MasterCfg == (DW_IC_CON_MASTER | DW_IC_CON_SLAVE_DISABLE |
                           DW_IC_CON_RESTART_EN | DW_IC_CON_SPEED_FAST));
    check("CON register written", load(DW_IC_CON) == hw.MasterCfg);
    check("SDA_HOLD RX-hold workaround set (bit16)", (load(DW_IC_SDA_HOLD) & DW_IC_SDA_HOLD_RX_MASK) == (1u << DW_IC_SDA_HOLD_RX_SHIFT));

    DwI2cHwSetTarget(&hw, 0x50);
    check("TAR == 0x50", load(DW_IC_TAR) == 0x50);
    check("ENABLE set after SetTarget", load(DW_IC_ENABLE) == DW_IC_ENABLE_ENABLE);

    /* write [0xAB,0xCD] with STOP on last */
    g_txN = 0;
    st = DwI2cHwWritePolled(&hw, wbuf, 2, TRUE);
    check("WritePolled succeeds", NT_SUCCESS(st));
    check("2 DATA_CMD writes", g_txN == 2);
    check("byte0 == 0xAB, no STOP", (g_txLog[0] & 0xFF) == 0xAB && !(g_txLog[0] & DW_IC_DATA_CMD_STOP));
    check("byte1 == 0xCD, with STOP", (g_txLog[1] & 0xFF) == 0xCD && (g_txLog[1] & DW_IC_DATA_CMD_STOP));

    /* read 2 bytes with STOP on last */
    g_txN = 0; g_rxHead = g_rxTail = 0; g_rxSrcN = 0;
    st = DwI2cHwReadPolled(&hw, rbuf, 2, TRUE);
    check("ReadPolled succeeds", NT_SUCCESS(st));
    check("2 read requests issued", g_txN == 2);
    check("read req has READ bit", (g_txLog[0] & DW_IC_DATA_CMD_READ) != 0);
    check("last read req has STOP", (g_txLog[1] & DW_IC_DATA_CMD_STOP) != 0);
    check("rbuf[0] == 0x11", rbuf[0] == 0x11);
    check("rbuf[1] == 0x22", rbuf[1] == 0x22);

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
