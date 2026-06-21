/*++ adc_hw.c - RP1 ADC HAL (ported from rp1-adc.c). --*/
#include "adc_regio.h"
#include "adc_hw.h"

#define ADC_POLL_MAX 100000

void AdcHwEnable(void *Base, int EnableTempSensor)
{
    /* rp1_adc_probe: disable interrupts, then enable the block while clearing any
     * sticky error (CS_ERR_STICKY is W1C). Prior code RMW'd only CS_EN, so a
     * boot-time sticky error would mark every later conversion as failed. */
    unsigned cs = RP1_ADC_CS_EN | RP1_ADC_CS_ERR_STICKY;
    if (EnableTempSensor) {
        cs |= RP1_ADC_CS_TS_EN;
    }
    WR32(Base, RP1_ADC_INTE, 0);
    WR32(Base, RP1_ADC_CS, cs);
}

void AdcHwSelectChannel(void *Base, unsigned Channel)
{
    /* Use the atomic CLR/SET alias windows (rp1_adc_read) instead of a RMW on CS,
     * which also carries the live READY/ERR status bits. Clear AINSEL, set channel. */
    WR32(Base, RP1_ADC_RWTYPE_CLR + RP1_ADC_CS,
         RP1_ADC_CS_AINSEL_MASK << RP1_ADC_CS_AINSEL_SHIFT);
    WR32(Base, RP1_ADC_RWTYPE_SET + RP1_ADC_CS,
         (Channel & RP1_ADC_CS_AINSEL_MASK) << RP1_ADC_CS_AINSEL_SHIFT);
}

int AdcHwIsReady(void *Base)
{
    return (RD32(Base, RP1_ADC_CS) & RP1_ADC_CS_READY) ? 1 : 0;
}

unsigned AdcHwReadResult(void *Base)
{
    return RD32(Base, RP1_ADC_RESULT) & RP1_ADC_RESULT_MASK;
}

int AdcHwConvert(void *Base, unsigned Channel, unsigned *Result)
{
    unsigned i;
    AdcHwEnable(Base, 0);
    AdcHwSelectChannel(Base, Channel);
    WR32(Base, RP1_ADC_RWTYPE_SET + RP1_ADC_CS, RP1_ADC_CS_START_ONCE);  /* atomic set */
    for (i = 0; i < ADC_POLL_MAX; i++) {
        if (AdcHwIsReady(Base)) {
            break;
        }
    }
    if (i == ADC_POLL_MAX) {
        return -1;
    }
    *Result = AdcHwReadResult(Base);
    return 0;
}
