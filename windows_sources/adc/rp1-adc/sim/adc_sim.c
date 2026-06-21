/*++ adc_sim.c - x64 simulation of the RP1 ADC HAL. Build: cl /DADC_SIM /I.. adc_sim.c ..\adc_hw.c --*/
#include <stdio.h>
#include <string.h>
#include "../adc_hw.h"

unsigned char *g_adcBase;
static unsigned char g_regs[0x40];
static unsigned load(unsigned off)         { unsigned v=0; int i; for(i=0;i<4;i++) v|=((unsigned)g_regs[off+i])<<(8*i); return v; }
static void     store(unsigned off,unsigned v){ int i; for(i=0;i<4;i++) g_regs[off+i]=(unsigned char)((v>>(8*i))&0xFF); }
unsigned int AdcSimRd(unsigned off)        { return load(off); }
static void adcStartIfOnce(unsigned cs)
{
    if (cs & RP1_ADC_CS_START_ONCE) {   /* a conversion completes -> READY + result */
        store(RP1_ADC_CS, load(RP1_ADC_CS) | RP1_ADC_CS_READY);
        store(RP1_ADC_RESULT, 0x5A5);
    }
}
void AdcSimWr(unsigned off, unsigned val)
{
    /* model the atomic SET/CLR alias windows (rp1-adc.c) */
    if (off == RP1_ADC_RWTYPE_SET + RP1_ADC_CS) {       /* 0x2000: CS |= val */
        store(RP1_ADC_CS, load(RP1_ADC_CS) | val);
        adcStartIfOnce(val);
        return;
    }
    if (off == RP1_ADC_RWTYPE_CLR + RP1_ADC_CS) {       /* 0x3000: CS &= ~val */
        store(RP1_ADC_CS, load(RP1_ADC_CS) & ~val);
        return;
    }
    store(off, val);
    if (off == RP1_ADC_CS) adcStartIfOnce(val);         /* direct CS write */
}

static int g_pass, g_fail;
static void check(const char *what, int c){ if(c){g_pass++;printf("  [PASS] %s\n",what);}else{g_fail++;printf("  [FAIL] %s\n",what);} }

int main(void)
{
    unsigned r;
    int rc;
    memset(g_regs, 0, sizeof(g_regs));
    g_adcBase = g_regs;
    printf("== RP1 ADC HAL simulation ==\n");

    AdcHwEnable(g_regs, 1);
    check("CS has EN|TS_EN", (load(RP1_ADC_CS) & (RP1_ADC_CS_EN|RP1_ADC_CS_TS_EN)) == (RP1_ADC_CS_EN|RP1_ADC_CS_TS_EN));
    check("enable clears sticky error (CS_ERR_STICKY written)", (load(RP1_ADC_CS) & RP1_ADC_CS_ERR_STICKY) != 0);
    check("enable disabled interrupts (INTE==0)", load(RP1_ADC_INTE) == 0);

    /* select via atomic alias: set channel 3 then 5, verify CLR then SET worked */
    AdcHwSelectChannel(g_regs, 3);
    check("CS AINSEL field == 3 (alias set)", ((load(RP1_ADC_CS) >> RP1_ADC_CS_AINSEL_SHIFT) & RP1_ADC_CS_AINSEL_MASK) == 3);
    AdcHwSelectChannel(g_regs, 5);
    check("CS AINSEL re-selected == 5 (alias clr+set)", ((load(RP1_ADC_CS) >> RP1_ADC_CS_AINSEL_SHIFT) & RP1_ADC_CS_AINSEL_MASK) == 5);

    store(RP1_ADC_CS, RP1_ADC_CS_READY);  /* ready latched */
    check("IsReady true", AdcHwIsReady(g_regs) == 1);
    store(RP1_ADC_RESULT, 0xABC);
    check("ReadResult masks to 12-bit (0xABC)", AdcHwReadResult(g_regs) == 0xABC);

    /* full one-shot convert sequence */
    memset(g_regs, 0, sizeof(g_regs));
    rc = AdcHwConvert(g_regs, 2, &r);
    check("Convert returns 0", rc == 0);
    check("Convert result == 0x5A5 (modeled)", r == 0x5A5);
    check("Convert selected channel 2", ((load(RP1_ADC_CS) >> RP1_ADC_CS_AINSEL_SHIFT) & RP1_ADC_CS_AINSEL_MASK) == 2);
    check("Convert issued START_ONCE", (load(RP1_ADC_CS) & RP1_ADC_CS_START_ONCE) != 0);

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
