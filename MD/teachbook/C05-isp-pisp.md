# C5：ISP（PiSP Back End，AVStream / MFT）

| | |
|---|---|
| **裝置** | BCM2712 PiSP Back End（影像訊號處理器） |
| **Linux 源碼** | `drivers/media/platform/raspberrypi/pisp_be/pisp_be.c` |
| **Windows 框架** | **AVStream / MFT**（影像處理流水線） |
| **本專案** | 規劃中（🔲，Tier 3） |

## 1. ISP 在相機流水線的位置

```
[CSI 相機前端(C6)] → RAW frame → [ISP/PiSP(本章)] → 去馬賽克/白平衡/降噪/色彩 → 可用影像 → 應用
```
ISP 把感測器的 RAW Bayer 資料處理成標準影像（YUV/RGB）。

## 2. 移植要點

- PiSP 是**可程式化流水線**：要載入處理參數（config block）+ 設輸入/輸出 buffer + 啟動。
- Windows 端：可做成 AVStream filter 的一段，或 MFT；與 [CSI（C6）](C06-csi-camera.md) 串接。
- 屬 Tier 3，工程量大（影像演算法參數 + DMA buffer 管理）。

➡️ 回 [目錄](README.md)
