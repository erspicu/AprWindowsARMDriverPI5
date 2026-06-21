/*++
Module Name: bcmi2c_sim.c
Abstract:    x64 simulation of the BCM2835/BCM2712 BSC I2C HAL (bcm_i2c_hw.c).
             Models the BSC start/status/FIFO handshake so WritePolled/ReadPolled
             can be exercised and asserted without hardware.
             Build: cl /DBCMI2C_SIM /I.. bcmi2c_sim.c ..\bcm_i2c_hw.c
--*/
#include <stdio.h>
#include "../bcm_i2c_hw.h"

unsigned char *g_bcmBase;
static unsigned char g_regs[0x40];
static unsigned g_fifoLog[64]; static int g_fifoN;       /* FIFO writes */
static const unsigned char g_rxSrc[] = { 0xDE, 0xAD, 0xBE, 0xEF };

/* transfer model */
static int g_mode;    /* 0=idle, 1=write, 2=read */
static int g_len, g_wcount, g_rcount;

static unsigned load(unsigned off)         { unsigned v=0; int i; for(i=0;i<4;i++) v|=((unsigned)g_regs[off+i])<<(8*i); return v; }
static void     store(unsigned off,unsigned v){ int i; for(i=0;i<4;i++) g_regs[off+i]=(unsigned char)((v>>(8*i))&0xFF); }

unsigned int BcmSimRd(unsigned off)
{
    if (off == BCM_I2C_S) {
        unsigned s = 0;
        if (g_mode == 1) { s |= (g_wcount < g_len) ? BCM_I2C_S_TXD : BCM_I2C_S_DONE; }
        else if (g_mode == 2) { s |= (g_rcount < g_len) ? BCM_I2C_S_RXD : BCM_I2C_S_DONE; }
        return s;
    }
    if (off == BCM_I2C_FIFO) {                            /* read pops slave data */
        unsigned char b = g_rxSrc[g_rcount % (int)sizeof(g_rxSrc)];
        g_rcount++;
        return b;
    }
    return load(off);
}

void BcmSimWr(unsigned off, unsigned val)
{
    if (off == BCM_I2C_C) {
        store(off, val);
        if (val & BCM_I2C_C_ST) {                         /* start transfer */
            g_len    = (int)load(BCM_I2C_DLEN);
            g_mode   = (val & BCM_I2C_C_READ) ? 2 : 1;
            g_wcount = 0;
            g_rcount = 0;
        }
        return;
    }
    if (off == BCM_I2C_FIFO) {                            /* write pushes a byte */
        if (g_fifoN < 64) g_fifoLog[g_fifoN++] = val;
        g_wcount++;
        return;
    }
    store(off, val);
}

static int g_pass, g_fail;
static void check(const char *what, int cond)
{
    if (cond) { g_pass++; printf("  [PASS] %s\n", what); }
    else      { g_fail++; printf("  [FAIL] %s\n", what); }
}

int main(void)
{
    BCM_I2C_HW hw;
    NTSTATUS   st;
    unsigned char wbuf[3] = { 0x01, 0x02, 0x03 };
    unsigned char rbuf[2] = { 0, 0 };

    memset(g_regs, 0, sizeof(g_regs));
    g_bcmBase = g_regs;
    printf("== BCM BSC I2C transfer-engine simulation ==\n");

    BcmI2cHwInit(&hw, g_regs, sizeof(g_regs));
    BcmI2cHwSetDivider(&hw, 0);                          /* default */
    check("default DIV == 0x438 (100kHz @108MHz)", load(BCM_I2C_DIV) == 0x0438);
    BcmI2cHwSetDivider(&hw, 0x05DC);                     /* explicit override honoured */
    check("explicit DIV honoured (0x05DC)", load(BCM_I2C_DIV) == 0x05DC);
    /* DIV-from-Hz @ core=108 MHz (Pi5-measured). */
    check("DivForHz(0) -> default 0x438", BcmI2cHwDivForHz(0)      == 0x0438);
    check("DivForHz(100kHz) == 1080",     BcmI2cHwDivForHz(100000) == 1080);
    check("DivForHz(400kHz) == 270",      BcmI2cHwDivForHz(400000) == 270);

    BcmI2cHwSetTarget(&hw, 0x68);
    check("TargetAddr == 0x68", hw.TargetAddr == 0x68);

    /* write 3 bytes */
    g_fifoN = 0; g_mode = 0;
    st = BcmI2cHwWritePolled(&hw, wbuf, 3);
    check("WritePolled succeeds", NT_SUCCESS(st));
    check("slave addr A == 0x68", load(BCM_I2C_A) == 0x68);
    check("DLEN == 3", load(BCM_I2C_DLEN) == 3);
    check("3 FIFO writes", g_fifoN == 3);
    check("FIFO bytes 01 02 03", (g_fifoLog[0]&0xFF)==0x01 && (g_fifoLog[1]&0xFF)==0x02 && (g_fifoLog[2]&0xFF)==0x03);
    check("last C had ST, no READ", (load(BCM_I2C_C) & (BCM_I2C_C_ST|BCM_I2C_C_READ)) == BCM_I2C_C_ST);

    /* read 2 bytes */
    g_mode = 0;
    st = BcmI2cHwReadPolled(&hw, rbuf, 2);
    check("ReadPolled succeeds", NT_SUCCESS(st));
    check("DLEN == 2", load(BCM_I2C_DLEN) == 2);
    check("last C had ST|READ", (load(BCM_I2C_C) & (BCM_I2C_C_ST|BCM_I2C_C_READ)) == (BCM_I2C_C_ST|BCM_I2C_C_READ));
    check("rbuf[0] == 0xDE", rbuf[0] == 0xDE);
    check("rbuf[1] == 0xAD", rbuf[1] == 0xAD);

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
