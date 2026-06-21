# HEVC 硬體解碼（rpivid）移植筆記

> Pi5（BCM2712）的 **HEVC(H.265) 硬體解碼塊 `rpivid`** 移植到 **Windows on ARM64** 的研究筆記。
> 來源：本專案分析 + Gemini 3 輪諮詢（`tools/knowledgebase/message/`）。建立：2026-06-22

## ⚠️ 先看：玩家期待管理（最重要的誠實事實）

> **「Pi5 有 HEVC 硬解」≠「能順看 YouTube 4K」。** 兩者用的 codec 不同。

| 情境 | Pi5 硬解能幫忙？ | 為什麼 |
|------|:---:|------|
| **本地 4K HEVC 影片檔**（.mkv/.mp4，H.265）| ✅ **能**（4Kp60 順）| 正是 `rpivid` 的用途 |
| H.265 IP Cam 串流 / Plex/Jellyfin 轉碼成 HEVC | ✅ 能 | 同上 |
| **YouTube 4K** | ❌ **完全用不到** | YouTube 4K = **VP9/AV1**，Pi5 **無** VP9/AV1 硬解 → CPU 軟解 |
| H.264 影片 | ❌ 用不到 | Pi5 **砍掉了** Pi4 的 H.264 硬解 → 軟解 |

- **Pi5 硬解能力 = HEVC 解碼 only**（無 H.264/VP9/AV1 硬解、無任何硬體編碼）。
- YouTube/一般串流軟解現實（A76×4）：1080p60 順、4K30 逼近滿載會掉幀、**4K60 跑不動**。
- 所以這塊的真正價值＝**本地 4K HEVC 電影**。

## 第二個誠實事實：做了 MFT，哪些播放器吃得到？

| 播放器 | 會用我的 MFT？ | 說明 |
|--------|:---:|------|
| **「電影與電視」/ Windows Media Player / UWP**（走 Media Foundation）| ✅ **會** | 註冊成功 + 吐標準 NV12 即自動接上 |
| **Edge / Chromium（YouTube）** | ❌ **不會** | 繞過 MFT、直查 D3D11 DXVA；沒 WDDM=沒 D3D11VideoDevice=看不到你 |
| **VLC / MPC-HC / Kodi** | ❌ 預設不會 | 核心是 FFmpeg，走 DXVA2/D3D11VA，**不吃 MFT**；要另寫 FFmpeg hwaccel 模組 |

> **結論**：MFT 路線能讓「Windows 原生播放器看本地 4K HEVC」；要涵蓋 VLC/Kodi 需另走 **FFmpeg hwaccel**（不是 MFT）。

## 路線

```
本地 HEVC 檔 → Media Foundation → [你的 Sync MFT]
   ├ ProcessInput: parse NALU → 餵 KMDF
   ├ KMDF(rpivid): 寫控制結構+slice PA → 觸發 → IRQ → 輸出 SAND
   └ ProcessOutput: SAND →(NEON)→ NV12 → 給 EVR 顯示
```

## 筆記索引

| 檔案 | 內容 |
|------|------|
| [`01-hardware-and-reality.md`](01-hardware-and-reality.md) | Pi5 解碼硬體事實、codec 涵蓋誠實評估、整合現實、工程量、參考 SOURCES |
| [`02-rpivid-kmdf-decode.md`](02-rpivid-kmdf-decode.md) | **KMDF driver + stateless 解碼流程**：MMIO/IRQ/IOCTL、控制結構、DPB/RPS、SAND layout、256MB common buffer、I-frame 驗證里程碑 |
| [`03-mft-and-integration.md`](03-mft-and-integration.md) | **MFT 實作 + 註冊 + 整合現實**：IMFTransform、`SYNCMFT` 偽裝、SAND→NV12、TopoEdit→電影與電視 里程碑 |

## 工程量
~**3.5-4.5 人月**（1 資深 Windows media/driver 工程師）。60% 工作在「對照 Linux rpivid 源碼還原沒公開的 MMIO offset / descriptor bitmask」。

## 現況
本專案難度清單把 HEVC 列為 🔴；本筆記給出獨立 MFT+KMDF 的可落地路線（不需完整 WDDM）。
