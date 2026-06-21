/*++
Module Name: gem_hw.h
Abstract:    Cadence GEM (Ethernet, "macb") HAL - register map, MAC/config,
             DMA descriptor ring, and MDIO. Ported from Linux drivers/net/
             ethernet/cadence/macb_main.c. Pure logic (no OS types) so it is
             verifiable by the x64 gem_sim harness.
--*/
#pragma once

/* ---- GEM register offsets (macb.h) ---- */
#define GEM_NCR     0x00   /* network control */
#define GEM_NCFGR   0x04   /* network config */
#define GEM_NSR     0x08   /* network status */
#define GEM_TSR     0x14   /* tx status */
#define GEM_RBQP    0x18   /* rx buffer queue base */
#define GEM_TBQP    0x1C   /* tx buffer queue base */
#define GEM_RSR     0x20   /* rx status */
#define GEM_ISR     0x24
#define GEM_IER     0x28
#define GEM_IDR     0x2C
#define GEM_MAN     0x34   /* PHY maintenance (MDIO) */
#define GEM_SA1B    0x88   /* specific address 1 bottom (bytes 0..3) */
#define GEM_SA1T    0x8C   /* specific address 1 top    (bytes 4..5) */

/* NCR (network control) */
#define GEM_NCR_RXEN   (1u << 2)
#define GEM_NCR_TXEN   (1u << 3)
#define GEM_NCR_MPE    (1u << 4)   /* management (MDIO) port enable */

/* NCFGR (network config) */
#define GEM_NCFGR_SPD  (1u << 0)   /* 100 Mbps */
#define GEM_NCFGR_FD   (1u << 1)   /* full duplex */
#define GEM_NCFGR_DRFCS (1u << 17) /* discard RX FCS (macb_init_hw always sets)  */
/* MDC clock divider, NCFGR[20:18] (GEM_CLK_OFFSET=18). gem_mdc_clk_div() picks
 * DIV96 (=5) for pclk 160-240 MHz; RP1 GEM pclk = clk_sys = 200 MHz (Pi5-measured,
 * confirmed by live NCFGR=0x01560048 -> CLK field = 5). Without it MDC defaults to
 * pclk/8 = 25 MHz, far above the 2.5 MHz MDIO max -> PHY access fails. */
#define GEM_NCFGR_CLK_DIV96 (5u << 18)

/* TX descriptor ctrl word */
#define GEM_TX_FRMLEN_MASK  0x3FFFu
#define GEM_TX_LAST   (1u << 15)
#define GEM_TX_WRAP   (1u << 30)
#define GEM_TX_USED   (1u << 31)   /* HW sets when sent; SW clears to enqueue */

/* RX descriptor: addr word flags + ctrl word fields */
#define GEM_RX_USED   (1u << 0)    /* in addr word: 1 = owned by SW (filled) */
#define GEM_RX_WRAP   (1u << 1)
#define GEM_RX_ADDR_MASK  0xFFFFFFFCu
#define GEM_RX_FRMLEN_MASK 0x1FFFu /* in ctrl word */
#define GEM_RX_SOF    (1u << 14)
#define GEM_RX_EOF    (1u << 15)

/* MAN (MDIO) opcodes */
#define GEM_MAN_READ   2
#define GEM_MAN_WRITE  1

typedef struct _GEM_DESC {
    unsigned int addr;
    unsigned int ctrl;
} GEM_DESC;

void GemHwReset(void *Base);
void GemSetMacAddr(void *Base, const unsigned char Mac[6]);
void GemConfigure(void *Base, int FullDuplex, int Speed100);
void GemSetRings(void *Base, unsigned int RxRingPhys, unsigned int TxRingPhys);
void GemEnable(void *Base);

void         GemTxBuildDesc(GEM_DESC *Desc, unsigned int BufPhys, unsigned int Len, int Last, int Wrap);
void         GemRxInitDesc(GEM_DESC *Desc, unsigned int BufPhys, int Wrap);
int          GemRxDescIsUsed(const GEM_DESC *Desc);
unsigned int GemRxDescLength(const GEM_DESC *Desc);
unsigned int GemMdioBuild(int Phy, int Reg, int IsWrite, unsigned int Data);
