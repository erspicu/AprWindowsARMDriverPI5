/*++
Module Name: pwm_hw.h
Abstract:    RP1 PWM HAL. Register map + channel config/duty/enable, ported from
             Linux drivers/pwm/pwm-rp1.c. Pure logic (void* Base), sim-verifiable.
--*/
#pragma once

#define PWM_GLOBAL_CTRL        0x000
#define PWM_CHANNEL_CTRL(x)    (0x014 + (x) * 16)
#define PWM_RANGE(x)           (0x018 + (x) * 16)
#define PWM_DUTY(x)            (0x020 + (x) * 16)
#define PWM_CHANNEL_DEFAULT    ((1u << 8) | (1u << 0))  /* FIFO_POP_MASK + trailing-edge M/S */
#define PWM_CHANNEL_ENABLE(x)  (1u << (x))
/* GLOBAL_CTRL SET_UPDATE: latch the staged duty/range/ctrl into the channel.
 * pwm-rp1.c writes this after every apply; without it the staged values never
 * take effect (PWM output stays unchanged). */
#define PWM_SET_UPDATE         (1u << 31)
#define RP1_PWM_CHANNELS       4

void PwmHwConfigChannel(void *Base, unsigned Channel);
void PwmHwSetDutyRange(void *Base, unsigned Channel, unsigned Duty, unsigned Range);
void PwmHwApplyConfig(void *Base);
void PwmHwEnable(void *Base, unsigned Channel, int Enable);
