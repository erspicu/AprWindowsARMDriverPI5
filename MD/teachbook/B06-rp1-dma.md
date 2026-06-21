# B6：RP1 DMA（Synopsys DesignWare AXI-DMA，bespoke KMDF）

> 和 [BCM2712 DMA40（A7）](A07-bcm2712-dma40.md) 是**兩顆完全不同的 DMA 引擎**——教「別把兩套 DMA 搞混」。

| | |
|---|---|
| **裝置** | RP1 DMA（Synopsys DesignWare AXI-DMAC） |
| **Linux 源碼** | `drivers/dma/dw-axi-dmac/dw-axi-dmac-platform.c` |
| **Windows 框架** | bespoke KMDF |
| **本專案** | 規劃中（🔲），ACPI 已 forward-declare（`RPI5000A`，IRQ 40） |

## 1. 兩套 DMA 別搞混

| | RP1 DMA（本章） | BCM2712 DMA（A7） |
|---|---|---|
| IP | Synopsys DesignWare AXI-DMAC | Broadcom DMA40 |
| 位址 | RP1 BAR1 `0x188000` | BCM SoC `0x1000010000` |
| IRQ | RP1 內部 **40**（Pi5 實測 `dw_axi_dmac_platform`） | GIC 119-122 |
| 暫存器 | DesignWare AXI-DMA 那套 | DMA40 那套 |

> 教學點：Pi5 有**兩種 DMA**，移植時要看「這個傳輸是哪顆 DMA 服務的」。ACPI 的 `DMA0`(RPI5000A) 是 RP1 這顆；
> BCM 那顆是另一個裝置（A7）。

## 2. DesignWare AXI-DMA 概念

- descriptor-based：每個 channel 有 control block 描述 src/dst/len/next。
- 設 channel 的 SAR/DAR/CTL，啟動，等完成中斷。
- 周邊 DMA 要設 handshake（哪個周邊的 DREQ 觸發）。

## 3. 移植要點

- 暫存器圖照 `dw-axi-dmac.h`。
- 與周邊（I2S/SPI/UART）的 DMA handshake 對應要對。
- 屬實機階段（DMA 時序、cache coherency、IOMMU 互動需 Pi5）。

➡️ 回 [目錄](README.md)
