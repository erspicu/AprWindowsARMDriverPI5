/*++
Module Name: i2s_sim.c
Abstract:    x64 simulation of the DesignWare I2S HAL (rp1_i2s_hw.c). Verifies
             Probe capability decode + config/start/stop/flush register writes.
             Build: cl /DI2S_SIM /I.. i2s_sim.c ..\rp1_i2s_hw.c
--*/
#include <stdio.h>
#include "../rp1_i2s_hw.h"

unsigned char *g_i2sBase;
static unsigned char g_regs[0x210];

static unsigned load(unsigned off)         { unsigned v=0; int i; for(i=0;i<4;i++) v|=((unsigned)g_regs[off+i])<<(8*i); return v; }
static void     store(unsigned off,unsigned v){ int i; for(i=0;i<4;i++) g_regs[off+i]=(unsigned char)((v>>(8*i))&0xFF); }

unsigned int I2sSimRd(unsigned off)
{
    if (off == DW_I2S_COMP_TYPE)    return DW_I2S_COMP_TYPE_MAGIC;
    if (off == DW_I2S_COMP_PARAM_1) return 0x000002FCu;   /* TX/RX en, MODE master, fifo16, 2 pairs */
    if (off == DW_I2S_COMP_PARAM_2) return 0;
    return load(off);
}
void I2sSimWr(unsigned off, unsigned val) { store(off, val); }

static int g_pass, g_fail;
static void check(const char *what, int cond)
{
    if (cond) { g_pass++; printf("  [PASS] %s\n", what); }
    else      { g_fail++; printf("  [FAIL] %s\n", what); }
}

int main(void)
{
    RP1I2S_HW hw;
    NTSTATUS  st;

    memset(g_regs, 0, sizeof(g_regs));
    g_i2sBase = g_regs;
    printf("== DesignWare I2S engine simulation ==\n");

    Rp1I2sHwInit(&hw, g_regs, sizeof(g_regs));
    check("Init: default resolution 16-bit (0x02)", hw.XferResolution == 0x02);

    st = Rp1I2sHwProbe(&hw);
    check("Probe succeeds (COMP_TYPE magic)", NT_SUCCESS(st));
    check("capability PLAY|RECORD|MASTER",
          (hw.Capability & (DWC_I2S_PLAY | DWC_I2S_RECORD | DW_I2S_MASTER))
           == (DWC_I2S_PLAY | DWC_I2S_RECORD | DW_I2S_MASTER));
    check("FifoDepth == 16", hw.FifoDepth == 16);
    check("FifoTh == 8", hw.FifoTh == 8);
    check("TxChannelsMax == 4", hw.TxChannelsMax == 4);

    st = Rp1I2sHwSetResolution(&hw, 24);
    check("SetResolution(24) -> 0x04", NT_SUCCESS(st) && hw.XferResolution == 0x04);
    check("SetResolution(24) -> Ccr 0x08", hw.Ccr == 0x08);
    check("SetResolution(99) rejected", Rp1I2sHwSetResolution(&hw, 99) != STATUS_SUCCESS);
    check("SetResolution(16) -> Ccr 0x00", Rp1I2sHwSetResolution(&hw, 16) == STATUS_SUCCESS && hw.Ccr == 0x00);
    check("SetResolution(32) -> Ccr 0x10", Rp1I2sHwSetResolution(&hw, 32) == STATUS_SUCCESS && hw.Ccr == 0x10);
    (void)Rp1I2sHwSetResolution(&hw, 24);   /* restore 24-bit for the config checks */

    /* configure playback, stereo (1 channel pair) */
    Rp1I2sHwConfig(&hw, TRUE, 2);
    check("TCR(0) == resolution(0x04)", load(DW_I2S_TCR(0)) == 0x04);
    check("CCR written == 0x08 (24-bit word-select)", load(DW_I2S_CCR) == 0x08);
    check("TFCR(0) == depth-th-1 == 7", load(DW_I2S_TFCR(0)) == (16 - 8 - 1));
    check("TER(0) enabled", load(DW_I2S_TER(0)) == DW_I2S_TER_TXCHEN);
    check("DMACR has TXCH0 enable", (load(DW_I2S_DMACR) & DW_I2S_DMACR_DMAEN_TXCH0) != 0);

    /* seed IMR with TX bits masked, to verify start unmasks them */
    store(DW_I2S_IMR(0), 0x33);

    /* start playback */
    Rp1I2sHwStart(&hw, TRUE);
    check("IER global enable", load(DW_I2S_IER) == DW_I2S_IER_IEN);
    check("ITER tx-block enable", load(DW_I2S_ITER) == 1);
    check("CER clock enable", load(DW_I2S_CER) == 1);
    check("DMACR TXBLOCK set", (load(DW_I2S_DMACR) & DW_I2S_DMAEN_TXBLOCK) != 0);
    check("start unmasks TX FIFO IRQs (IMR&0x30==0)", (load(DW_I2S_IMR(0)) & 0x30) == 0);
    check("start preserves RX mask bits (0x03 kept)", (load(DW_I2S_IMR(0)) & 0x03) == 0x03);

    /* stop playback */
    Rp1I2sHwStop(&hw, TRUE);
    check("ITER cleared on stop", load(DW_I2S_ITER) == 0);
    check("CER cleared on stop", load(DW_I2S_CER) == 0);
    check("TER(0) disabled on stop", load(DW_I2S_TER(0)) == 0);
    check("stop masks TX FIFO IRQs (IMR&0x30==0x30)", (load(DW_I2S_IMR(0)) & 0x30) == 0x30);

    /* flush */
    Rp1I2sHwFlush(&hw, TRUE);
    check("TXFFR fifo reset", load(DW_I2S_TXFFR) == 1);

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
