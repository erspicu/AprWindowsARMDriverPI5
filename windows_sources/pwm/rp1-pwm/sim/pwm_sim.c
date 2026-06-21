/*++ pwm_sim.c - x64 simulation of the RP1 PWM HAL. Build: cl /DPWM_SIM /I.. pwm_sim.c ..\pwm_hw.c --*/
#include <stdio.h>
#include <string.h>
#include "../pwm_hw.h"

unsigned char *g_pwmBase;
static unsigned char g_regs[0x80];
static unsigned load(unsigned off)         { unsigned v=0; int i; for(i=0;i<4;i++) v|=((unsigned)g_regs[off+i])<<(8*i); return v; }
static void     store(unsigned off,unsigned v){ int i; for(i=0;i<4;i++) g_regs[off+i]=(unsigned char)((v>>(8*i))&0xFF); }
unsigned int PwmSimRd(unsigned off)        { return load(off); }
void         PwmSimWr(unsigned off, unsigned val){ store(off, val); }

static int g_pass, g_fail;
static void check(const char *what, int c){ if(c){g_pass++;printf("  [PASS] %s\n",what);}else{g_fail++;printf("  [FAIL] %s\n",what);} }

int main(void)
{
    memset(g_regs, 0, sizeof(g_regs));
    g_pwmBase = g_regs;
    printf("== RP1 PWM HAL simulation ==\n");

    PwmHwConfigChannel(g_regs, 0);
    check("CH0 CTRL(@0x014) == DEFAULT (FIFO_POP|trailing M/S)", load(PWM_CHANNEL_CTRL(0)) == PWM_CHANNEL_DEFAULT);

    PwmHwSetDutyRange(g_regs, 0, 50, 100);
    check("CH0 DUTY(@0x020) == 50", load(PWM_DUTY(0)) == 50);
    check("CH0 RANGE(@0x018) == 100", load(PWM_RANGE(0)) == 100);
    check("SET_UPDATE latched after SetDutyRange", (load(PWM_GLOBAL_CTRL) & PWM_SET_UPDATE) != 0);

    PwmHwEnable(g_regs, 0, 1);
    check("GLOBAL_CTRL bit0 set (CH0 enabled)", (load(PWM_GLOBAL_CTRL) & PWM_CHANNEL_ENABLE(0)) != 0);
    PwmHwEnable(g_regs, 2, 1);
    check("GLOBAL_CTRL bit2 set (CH2 enabled)", (load(PWM_GLOBAL_CTRL) & PWM_CHANNEL_ENABLE(2)) != 0);
    check("CH0 still enabled (independent)", (load(PWM_GLOBAL_CTRL) & PWM_CHANNEL_ENABLE(0)) != 0);
    PwmHwEnable(g_regs, 0, 0);
    check("GLOBAL_CTRL bit0 cleared (CH0 off)", (load(PWM_GLOBAL_CTRL) & PWM_CHANNEL_ENABLE(0)) == 0);
    check("CH2 still enabled", (load(PWM_GLOBAL_CTRL) & PWM_CHANNEL_ENABLE(2)) != 0);

    /* channel 1 uses a different register offset */
    PwmHwSetDutyRange(g_regs, 1, 25, 80);
    check("CH1 DUTY(@0x030) == 25", load(PWM_DUTY(1)) == 25);
    check("CH1 RANGE(@0x028) == 80", load(PWM_RANGE(1)) == 80);

    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
