/*++
Module Name: gem_hw.c
Abstract:    Cadence GEM HAL (MAC/config/ring/MDIO). Ported from macb_main.c.
--*/
#include "gem_regio.h"
#include "gem_hw.h"

void GemHwReset(void *Base)
{
    WR32(Base, GEM_NCR, 0);                 /* disable RX/TX */
    WR32(Base, GEM_IDR, 0xFFFFFFFFu);       /* mask all interrupts */
    WR32(Base, GEM_TSR, RD32(Base, GEM_TSR)); /* clear status (W1C) */
    WR32(Base, GEM_RSR, RD32(Base, GEM_RSR));
}

void GemSetMacAddr(void *Base, const unsigned char Mac[6])
{
    WR32(Base, GEM_SA1B, (unsigned)Mac[0] | ((unsigned)Mac[1] << 8) |
                         ((unsigned)Mac[2] << 16) | ((unsigned)Mac[3] << 24));
    WR32(Base, GEM_SA1T, (unsigned)Mac[4] | ((unsigned)Mac[5] << 8));
}

void GemConfigure(void *Base, int FullDuplex, int Speed100)
{
    /* Mirror macb_init_hw()'s unconditional NCFGR bits: MDC clock divider
     * (DIV96 @ 200 MHz pclk) + discard-RX-FCS. The MDC divider is essential -
     * without it MDIO PHY access runs at pclk/8 = 25 MHz (>> 2.5 MHz max). */
    unsigned int cfg = GEM_NCFGR_CLK_DIV96 | GEM_NCFGR_DRFCS;
    if (Speed100)   cfg |= GEM_NCFGR_SPD;
    if (FullDuplex) cfg |= GEM_NCFGR_FD;
    WR32(Base, GEM_NCFGR, cfg);
}

void GemSetRings(void *Base, unsigned int RxRingPhys, unsigned int TxRingPhys)
{
    WR32(Base, GEM_RBQP, RxRingPhys);
    WR32(Base, GEM_TBQP, TxRingPhys);
}

void GemEnable(void *Base)
{
    WR32(Base, GEM_NCR, GEM_NCR_RXEN | GEM_NCR_TXEN | GEM_NCR_MPE);
}

/* Build a TX descriptor: clear USED so the controller owns + transmits it. */
void GemTxBuildDesc(GEM_DESC *Desc, unsigned int BufPhys, unsigned int Len, int Last, int Wrap)
{
    Desc->addr = BufPhys;
    Desc->ctrl = (Len & GEM_TX_FRMLEN_MASK) |
                 (Last ? GEM_TX_LAST : 0u) |
                 (Wrap ? GEM_TX_WRAP : 0u);
    /* USED bit left clear => owned by the GEM DMA */
}

/* Init an RX descriptor: hand the buffer to HW by clearing the USED bit. */
void GemRxInitDesc(GEM_DESC *Desc, unsigned int BufPhys, int Wrap)
{
    Desc->addr = (BufPhys & GEM_RX_ADDR_MASK) | (Wrap ? GEM_RX_WRAP : 0u);
    Desc->ctrl = 0;
}

int GemRxDescIsUsed(const GEM_DESC *Desc)
{
    return (Desc->addr & GEM_RX_USED) ? 1 : 0;
}

unsigned int GemRxDescLength(const GEM_DESC *Desc)
{
    return Desc->ctrl & GEM_RX_FRMLEN_MASK;
}

/* Build a MAN register value for an MDIO read/write (Clause 22). */
unsigned int GemMdioBuild(int Phy, int Reg, int IsWrite, unsigned int Data)
{
    unsigned int op = IsWrite ? GEM_MAN_WRITE : GEM_MAN_READ;
    return (1u << 30)                       /* SOF = 01 */
         | (op << 28)
         | (((unsigned)Phy & 0x1F) << 23)
         | (((unsigned)Reg & 0x1F) << 18)
         | (2u << 16)                       /* code = 10 */
         | (Data & 0xFFFFu);
}
