/*++
Module Name: adc_hw.h
Abstract:    RP1 ADC + temperature-sensor HAL. Register map + one-shot convert,
             ported from Linux drivers/hwmon/rp1-adc.c. Pure logic, sim-able.
--*/
#pragma once

#define RP1_ADC_CS              0x00
#define RP1_ADC_RESULT          0x04
#define RP1_ADC_DIV             0x10
#define RP1_ADC_INTE            0x18
/* Atomic register-access alias windows (rp1-adc.c): write to base+SET/CLR+reg to
 * atomically set/clear bits without a read-modify-write race on live status bits. */
#define RP1_ADC_RWTYPE_SET      0x2000
#define RP1_ADC_RWTYPE_CLR      0x3000
#define RP1_ADC_CS_AINSEL_MASK  0x7
#define RP1_ADC_CS_AINSEL_SHIFT 12
#define RP1_ADC_CS_ERR_STICKY   0x400
#define RP1_ADC_CS_ERR          0x200
#define RP1_ADC_CS_READY        0x100
#define RP1_ADC_CS_START_ONCE   0x4
#define RP1_ADC_CS_TS_EN        0x2
#define RP1_ADC_CS_EN           0x1
#define RP1_ADC_RESULT_MASK     0xFFF   /* 12-bit result */
#define RP1_ADC_CHANNELS        8

void     AdcHwEnable(void *Base, int EnableTempSensor);
void     AdcHwSelectChannel(void *Base, unsigned Channel);
int      AdcHwIsReady(void *Base);
unsigned AdcHwReadResult(void *Base);
int      AdcHwConvert(void *Base, unsigned Channel, unsigned *Result);  /* 0 ok, -1 timeout */
