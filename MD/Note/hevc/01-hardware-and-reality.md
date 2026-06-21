# HEVC 移植：硬體事實 + 整合現實 + 參考

## 1. Pi5 BCM2712 影片解碼硬體（釐清事實）

| 項目 | 事實 |
|------|------|
| HEVC (H.265) 解碼 | ✅ 有硬解，最高 **4Kp60** |
| H.264 / MPEG 解碼 | ❌ **無**（Pi5 為省 silicon 面積，砍掉了 Pi4 的 H.264/MPEG 硬解）|
| VP9 / AV1 解碼 | ❌ **無** |
| 硬體編碼（任何 codec）| ❌ **無** |
| 代號 / 架構 | **`rpivid`**，**stateless 解碼器** |
| Linux 驅動 | `drivers/staging/media/rpi/rpivid/`（V4L2 stateless HEVC API）|

**Stateless 的意義**：硬體很笨——不懂容器、不懂完整 bitstream。**Host CPU（你的 Windows 驅動/MFT）必須**自己 parse SPS/PPS/Slice header、管理 reference frame/DPB，把算好的控制結構 + slice data 餵給硬體算像素。

## 2. 玩家情境的殘酷現實

- **本地 4K HEVC 檔**：`rpivid` 唯一能發揮的情境，4Kp60 順。
- **YouTube 4K**：早已全面 VP9/AV1 → **`rpivid` 100% 用不到** → CPU 軟解。
- **軟解能力（A76×4 @ 2.4GHz，推測）**：1080p60 順；4K30 VP9/AV1 逼近滿載、易掉幀+發熱降頻；4K60 像 PPT。
- **涵蓋 / 不涵蓋**：涵蓋＝本地 4K HEVC 電影、H.265 IP cam、Plex/Jellyfin 轉碼 HEVC 串流。**不涵蓋 YouTube 4K、H.264 影片**。

## 3. Windows 整合策略（為何走獨立 MFT）

- **傳統 DXVA2 / D3D11 Video 的困境**：標準硬解走 MF/D3D11 Video → WDDM 的 KMD/UMD。我們**沒有 V3D 的完整 WDDM**，HEVC 塊又是**獨立硬體**（不在 GPU 裡）→ 走 DXVA DDI 註冊不現實。
- **獨立 MFT 策略（可行）**：**Hardware-backed Sync MFT（user-mode）+ KMDF driver**，繞過 GPU 驅動。KMDF map rpivid MMIO + IRQ + IOCTL；MFT 實作 `IMFTransform`，內含 HEVC parser + DPB，餵 KMDF。詳見 [`02`](02-rpivid-kmdf-decode.md)/[`03`](03-mft-and-integration.md)。

### 兩個致命未知數
1. **SAND 輸出格式**：rpivid 輸出 Broadcom **SAND（column-based tiled）YUV**，非 linear NV12。Linux 上交給 ISP/V3D 轉；Windows 無 ISP 驅動 → 得用 **CPU NEON** 轉，4Kp60 可能因轉換瓶頸降到 4K20。
2. **瀏覽器 zero-copy**：Chromium 靠 D3D11VideoDevice(DXVA) 做 zero-copy → **極可能無視 system-memory MFT**，退回軟解。

## 4. 整合現實（哪些播放器吃得到）—— 見 [`03` §3](03-mft-and-integration.md)
- ✅ 電影與電視 / WMP / UWP / `IMFMediaEngine`（純 MF）。
- ❌ Edge/Chromium（繞過 MFT 走 DXVA）、VLC/MPC/Kodi（FFmpeg 走 DXVA，不吃 MFT）。
- 要涵蓋 VLC/Kodi：正規做法是寫 **FFmpeg hwaccel 模組**接 KMDF（不是 MFT）。

## 5. 工程量 + 參考

**~3.5-4.5 人月**：KMDF 讀寫+中斷 2-3 週；HEVC stateless parser+DPB 移植 1-1.5 月（最燒腦，除 reference frame 爛圖）；MFT 封裝+SAND→NV12 1 月；MF 整合填坑 1 月。

| 參考 | 用途 |
|------|------|
| Linux `drivers/staging/media/rpi/rpivid/` | **暫存器 + stateless 邏輯**（還原 MMIO offset/descriptor bitmask）|
| FFmpeg `v4l2_request` HEVC / LibreELEC/Kodi rpivid 支援 | bitstream → V4L2 控制結構的轉換邏輯 |
| Windows-classic-samples `multimedia/mediafoundation/decoder` | MFT 基本骨架 |
| `DXVA_PicParams_HEVC`（MS spec）| 雖不走 DXVA，但結構與硬體要的參數極相似，理解 DPB 怎麼餵硬體 |
