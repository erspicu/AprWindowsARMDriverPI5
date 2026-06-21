# HDMI 音訊移植筆記

> Pi5（BCM2712）**HDMI 音訊**移植到 Windows on ARM64。HDMI 顯示見 [`../gpu/`](../gpu/)（DOD）。
> 來源：本專案分析 + Gemini 1 輪諮詢。建立：2026-06-22

## 為什麼它是獨立一塊（不是顯示的副產品）

HDMI 音訊封包**嵌在 video TMDS/FRL 的 data island**裡，跟 HDMI 控制器綁死：要顯示先通、ACR 時脈跟 pixel clock、
audio enable。所以**不會因為顯示/GPU/PortCls 各別做完就自動出現**——要專門把這條接起來。

## Pi5 為什麼重要
> **Pi5 沒有 3.5mm 類比孔** → 內建出聲只剩 **HDMI 音訊 / USB 音效 / 藍牙音訊**。HDMI 音訊是**主要內建管道**，不是 niche。

## 架構一句話
```
Windows 音訊引擎 → [你的 PortCls + WaveRT miniport(.sys)]
   ├ 管 DMA：System RAM PCM →(BCM2712 DMA)→ MAI FIFO   ← 音訊驅動自己做
   └ 經 private interface 叫 DOD 設 ACR(N/CTS)/InfoFrame/MAI-enable ← DOD 擁有 HDMI MMIO
                              ↓
        HDMI TX audio packetizer → data island → 螢幕喇叭
```
- **DMA 走 BCM2712（不經 RP1）→ 不碰 PCIe/IOMMU**（跟相機不同）。
- HDMI 核心 = Synopsys DesignWare HDMI TX IP + Broadcom vc4 wrapper（延續 Pi4）。

## 關鍵設計：與 DOD 顯示驅動的耦合
音訊與顯示是**兩個獨立 `.sys`**。**不要讓音訊驅動自己 map HDMI MMIO**（race condition）。用 **kernel-mode private
interface**（像 Intel Display Audio）：DOD 擁有 HDMI 暫存器並暴露 function pointers（`IRP_MN_QUERY_INTERFACE`+自訂
GUID），音訊驅動呼叫 `GetPixelClock()` / `GetDisplayStatus()` / `SetAudioState(rate,ch)`，由 **DOD 去寫 ACR/InfoFrame/MAI**。

## 務實判斷
- 做法：**自寫 PortCls + WaveRT miniport**（SoC 標準，同高通 WoA）。**別偽裝 HDA codec**（要虛擬 PCIe+HDA controller+codec，工程巨大）。
- 工程量 **~2-4 人月**。最大坑：WaveRT DMA position 精準度（glitch）、DOD↔音訊的**電源狀態同步**（S3/螢幕關，易死鎖/藍屏）。
- 先有聲音的替代：USB 音效卡（inbox 即插即用）、藍牙音訊（[`../bluetooth/`](../bluetooth/)）、I2S DAC HAT。

## 筆記索引
| 檔案 | 內容 |
|------|------|
| [`01-implementation.md`](01-implementation.md) | 資料路徑/ACR/InfoFrame、PortCls/WaveRT、DOD private interface、DMA、里程碑（含「嗶一聲」）、參考 |

## 第一個里程碑
**M3「嗶一聲」**：hardcode 1080p60/48kHz 的 N/CTS，DMA 一段 1kHz sine wave 進 MAI → **螢幕喇叭發出長音 = 硬體路徑通**。
