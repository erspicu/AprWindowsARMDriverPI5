# C4：HEVC 解碼器（Media Foundation / DXVA）

| | |
|---|---|
| **裝置** | BCM2712 HEVC（H.265）硬體解碼器 |
| **Linux 源碼** | `drivers/media/platform/raspberrypi/hevc_dec/hevc_d.c`（V4L2 stateless decoder） |
| **Windows 框架** | **Media Foundation Transform (MFT) / DXVA2** |
| **本專案** | 規劃中（🔲，Tier 3） |

## 1. 概念對應

| | Linux | Windows |
|---|---|---|
| 框架 | V4L2 stateless decoder | **DXVA / Media Foundation MFT** |
| 上層提供 | 解析好的 slice header + 參考幀 | 同（DXVA 也是 stateless：host 做 parsing） |
| 硬體做 | 反量化/反轉換/motion comp/環路濾波 | 同 |

> 好消息：兩邊都是 **stateless**（bitstream parsing 在 host 做，硬體只做運算核心）——
> 暫存器層的「餵 slice 參數 + 啟動解碼」邏輯可沿用。

## 2. 移植要點

- 暫存器層：設參考幀、slice 參數、輸出 surface，啟動，等完成。
- 框架層：實作 DXVA decoder 或 MFT，把 Windows 的解碼請求翻成上面的暫存器操作。
- 需與顯示/記憶體（DMA surface）整合，屬 Tier 3。

➡️ 回 [目錄](README.md)
