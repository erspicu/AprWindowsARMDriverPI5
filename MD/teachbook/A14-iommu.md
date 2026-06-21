# A14：IOMMU（DMA 位址轉換）

| | |
|---|---|
| **裝置** | BCM2712 IOMMU |
| **Linux 源碼** | `drivers/iommu/bcm2712-iommu.c` |
| **Windows 框架** | DMA remapping（高難度） |
| **本專案** | 規劃中（🔲，★★★★） |

## 1. IOMMU 是什麼

IOMMU 在「裝置 DMA」與「實體記憶體」之間做**位址轉換 + 保護**（類似 CPU 的 MMU，但給周邊用）。
有了它，周邊看到的是「IO 虛擬位址」，由 IOMMU 翻成實體位址。

## 2. 為什麼影響移植

- 若某些 DMA 路徑經過 IOMMU，驅動配 DMA buffer 時拿到的位址要經 IOMMU 對應——
  配錯 DMA 會存取到錯誤實體位址。
- Windows 有自己的 DMA remapping 模型；要把 BCM2712 IOMMU 接上去（或在不需要時繞過）。

## 3. 移植要點

- 先確認**哪些裝置的 DMA 真的經過 IOMMU**（不是全部）。
- 屬高難度 + 實機；初期可先處理不經 IOMMU 的路徑。

➡️ 回 [目錄](README.md)
