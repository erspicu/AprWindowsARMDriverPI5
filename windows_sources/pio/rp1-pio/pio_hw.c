/*++ pio_hw.c - RP1 PIO FIFO HAL (ported from rp1-pio.c). --*/
#include "pio_regio.h"
#include "pio_hw.h"

void PioHwPushTx(void *Base, unsigned Sm, unsigned Value)
{
    WR32(Base, RP1_PIO_FIFO_TX(Sm), Value);
}

unsigned PioHwPopRx(void *Base, unsigned Sm)
{
    return RD32(Base, RP1_PIO_FIFO_RX(Sm));
}
