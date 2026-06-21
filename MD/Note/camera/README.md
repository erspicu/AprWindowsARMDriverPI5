# 相機（CSI-2 / PiSP ISP）移植筆記

> Pi5（BCM2712 + RP1）相機移植到 **Windows on ARM64**。
> 來源：本專案分析 + Gemini 2 輪諮詢（`tools/knowledgebase/message/`）。建立：2026-06-22

## 硬體資料鏈

```
Sensor(IMX708, Bayer RAW) ─MIPI CSI-2─► RP1 CFE(Camera Front End, D-PHY+CSI-2 RX)
   ─PCIe DMA─► System RAM(RAW) ─► BCM2712 PiSP(FE/BE 硬體 ISP, M2M) ─► RAM(YUV/RGB)
```
- **必驅動**：sensor I2C（控 AE/AWB/曝光）、RP1 CFE（收 CSI-2 + DMA RAW 進 RAM）。
- PiSP（硬體 ISP demosaic/AWB/gamma）**可繞過**（見策略）。
- libcamera 在軟體跑 **3A（AE/AWB/AF）**，硬體只出 pixel + 統計。

## 一句話策略：繞過硬體 PiSP，走「CFE 收 RAW + DeviceMFT 軟體 debayer」

> 完整移植硬體 PiSP（FE+BE，M2M、暫存器未完全公開）工程巨大。**最務實**：kernel 只用 RP1 CFE 把 Bayer RAW
> DMA 進 RAM，**user-mode DeviceMFT（CPU/GPU debayer）**轉 NV12 + 跑簡易 3A。先求有畫面，畫質再說。

## Windows 框架
```
AVStream Capture Minidriver(.sys, kernel：驅 sensor/CFE，出自訂 RAW) 
   → DeviceMFT(user-mode：debayer RAW→NV12 + 3A) → Windows Frame Server → MF → App（相機/Teams/瀏覽器）
```

## ⚠️ 成敗關鍵：PCIe DMA / IOMMU（WoA）
RP1 在 PCIe 底下，CFE 要 DMA 寫 system RAM。**WoA 有嚴格 SMMU(IOMMU) 隔離**——
**不能**用 `MmAllocateContiguousMemory`+`MmGetPhysicalAddress`（會 SMMU fault/BugCheck）。
**必須**用 WDF DMA：`WdfCommonBufferCreate` + `WdfCommonBufferGetAlignedLogicalAddress`（拿 **IOVA**）寫進 CFE DMA_BASE。

## 筆記索引
| 檔案 | 內容 |
|------|------|
| [`01-strategy-and-references.md`](01-strategy-and-references.md) | 硬體鏈、AVStream/DeviceMFT 框架、ISP 決策（軟體）、風險（IOMMU/ACPI）、參考 SOURCES |
| [`02-implementation.md`](02-implementation.md) | **可落地**：sensor/CSI-2/CFE bring-up、AVStream miniport（buffer 流/ks.h 坑）、DeviceMFT 軟體 ISP+3A loop、WDF DMA/IOMMU、**5 步里程碑** |

## 工程量
~**6-9 人月**（含軟體 ISP）。最大坑：**WoA PCIe DMA/IOMMU**、ACPI 描述 RP1 下的 I2C/CFE、PiSP 暫存器未公開（若要硬體 ISP）。

## 第一個里程碑
**I2C 讀回 IMX708 Chip ID `0x0708`** = 電源/時脈/I2C 通（最先驗）。再 → CFE DMA 收到雜訊 RAW（硬體牆打穿）→ AVStream 出綠畫面 → DeviceMFT 粗 debayer 出會動的畫面。
