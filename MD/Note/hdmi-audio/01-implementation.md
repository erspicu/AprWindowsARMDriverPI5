# HDMI 音訊：可落地實作

> 承 [`README`](README.md)。MAI/ACR 暫存器一律標「需查 Linux vc4_hdmi_audio.c」。

## 1. BCM2712 HDMI 音訊硬體

### 資料路徑
```
Host PCM → System RAM →(BCM2712 AXI DMA)→ MAI(Multichannel Audio Interface) FIFO
  → HDMI TX audio packetizer → data island packets(插 video blanking) → HDMI sink
```
- **DMA 走 BCM2712 內建 DMA controller，完全不經 RP1 → 不碰 PCIe/IOMMU**。
- HDMI core = Synopsys DesignWare HDMI TX IP + Broadcom vc4 wrapper。
- Linux：`vc4_hdmi_audio.c`（MAI 暫存器配置）+ `hdmi-codec` framework（InfoFrame 封裝）。

### 必設參數（與 video 高度耦合）
| 參數 | 作用 |
|------|------|
| **ACR（N / CTS）** | audio clock regeneration。sink 用 N/CTS + pixel clock 還原 audio sample clock（如 48kHz）。**pixel clock 變，N/CTS 必須跟著變** |
| **Audio InfoFrame** | channel count(2.0/5.1)、sample rate、bit depth |
| **Channel Status** | IEC60958（S/PDIF）狀態位元，標示 PCM 或 compressed passthrough |

- Pi4→Pi5：IP 幾乎同，差 base/IRQ，可能 DMA control-block 格式/對齊微調（需查/實機）。

## 2. Windows 端：PortCls + WaveRT miniport

- **唯一務實作法 = PortCls + WaveRT**（SoC 音訊標準）；**別偽裝 HDA codec**。
- **WaveRT 適合 DMA**：配一塊 cyclic ring buffer，硬體 DMA 繞圈讀，驅動只定期更新 position 暫存器。
- **端點怎麼出現**：
  1. ACPI DSDT 為 MAI 定義 device node（如 `BrcmHdmiAudio`）。
  2. PortCls 驅動 match 該 ACPI ID。
  3. 註冊 KS filter → Windows Audio Endpoint Builder 偵測 → 「聲音」控制台出現播放裝置。
- 音訊驅動與顯示驅動是**兩個獨立 `.sys`**。

## 3. ★ 與 DOD 的耦合（kernel private interface）

> 音訊封包嵌在 video 裡，audio 驅動無法獨立運作（要知道 pixel clock 算 ACR；螢幕 D3 時音訊要停）。
> **不要讓 audio 驅動自己 map HDMI MMIO**（race）。用 private interface（同 Intel Display Audio）：

1. **DOD 擁有 HDMI MMIO**，初始化 HDMI controller。
2. DOD 透過 `IRP_MN_QUERY_INTERFACE` + 自訂 GUID 暴露 function pointers。
3. **Audio 驅動呼叫 DOD**：
   - `GetDisplayStatus()`：螢幕亮著？EDID 支援音訊？
   - `GetPixelClock()`：當前 pixel clock（算 ACR 用）。
   - `SetAudioState(Enable, SampleRate, Channels)`：由 **DOD 去寫 HDMI TX 的 ACR / InfoFrame / MAI enable**。
4. **Audio 驅動自己只管**：System RAM DMA buffer、打 BCM2712 DMA controller、餵 PCM 進 FIFO、回報 WaveRT position。

## 4. 工程量 / 風險 / 參考

- **~2-4 人月**（熟 WDM/DMA）。
- 最大未知數：① WaveRT DMA **position reporting 精準度**（不準會 glitch）② DOD↔audio 在 S3/S0ix/螢幕關的**電源同步**（易死鎖/藍屏）。
- 韌體幫不上：mailbox 太粗（無法即時改 InfoFrame/ACR），且 DMA/position/IRQ 都在 Windows CPU 端 → audio 暫存器自己打 MMIO（經 DOD），參考 `vc4_hdmi_audio.c`。

| 參考 | 用途 |
|------|------|
| MS **`sysvad` sample**（Tablet WaveRT miniport）| **PortCls/WaveRT 聖經**；把 dummy buffer 換成真 DMA |
| Linux `vc4_hdmi.c` / `vc4_hdmi_audio.c` | MAI/ACR 暫存器 magic number 與順序 |
| HDMI Spec 1.4b/2.0 | ACR（N/CTS 計算公式）、Audio InfoFrame 封包結構 |

## 5. 里程碑

| M | 動作 | 驗收 |
|---|------|------|
| **M1 假端點** | 編 `sysvad`、改 INF match ACPI ID、安裝 | 系統匣喇叭圖示出現「Pi5 HDMI Audio」，可選取播放（雖無聲，Audio Engine 在跑）|
| **M2 跨驅動 IPC** | audio 驅動 query DOD interface | 成功讀到當前 pixel clock（如 148.5MHz）|
| **M3 嗶一聲 🚀** | hardcode 1080p60/48kHz N/CTS，DMA 一段 1kHz sine wave 進 MAI | **螢幕喇叭發出「嗶——」長音** = 硬體路徑通 |
| **M4 WaveRT 整合** | 把 Windows 下發的 WaveRT ring buffer 丟給 DMA | 播放 Windows 開機音效成功 |
