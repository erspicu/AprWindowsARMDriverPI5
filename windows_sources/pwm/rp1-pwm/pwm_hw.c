/*++ pwm_hw.c - RP1 PWM HAL (ported from pwm-rp1.c). --*/
#include "pwm_regio.h"
#include "pwm_hw.h"

void PwmHwConfigChannel(void *Base, unsigned Channel)
{
    WR32(Base, PWM_CHANNEL_CTRL(Channel), PWM_CHANNEL_DEFAULT);
}

/* duty/range are clock-period counts (caller computes from ns / clk_period). */
void PwmHwSetDutyRange(void *Base, unsigned Channel, unsigned Duty, unsigned Range)
{
    WR32(Base, PWM_DUTY(Channel), Duty);
    WR32(Base, PWM_RANGE(Channel), Range);
    PwmHwApplyConfig(Base);             /* latch the staged duty/range (SET_UPDATE) */
}

/* Latch staged channel config (pwm-rp1.c writes GLOBAL_CTRL|SET_UPDATE after apply). */
void PwmHwApplyConfig(void *Base)
{
    unsigned v = RD32(Base, PWM_GLOBAL_CTRL);
    WR32(Base, PWM_GLOBAL_CTRL, v | PWM_SET_UPDATE);
}

void PwmHwEnable(void *Base, unsigned Channel, int Enable)
{
    unsigned v = RD32(Base, PWM_GLOBAL_CTRL);
    if (Enable) {
        v |= PWM_CHANNEL_ENABLE(Channel);
    } else {
        v &= ~PWM_CHANNEL_ENABLE(Channel);
    }
    WR32(Base, PWM_GLOBAL_CTRL, v | PWM_SET_UPDATE);  /* enable + latch in one write */
}
