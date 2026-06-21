# C2：HVS / PixelValve / MOP（顯示合成，WDDM）

> 顯示管線的「中段」——把 framebuffer 合成、輸出時序，餵給 [HDMI（C1）](C01-hdmi.md)。

| | |
|---|---|
| **裝置** | BCM2712 HVS（Hardware Video Scaler / 合成）、PixelValve、MOP/MOPLET |
| **Linux 源碼** | `drivers/gpu/drm/vc4/vc4_hvs.c`、`vc4_*` |
| **Windows 框架** | **WDDM** |
| **本專案** | 規劃中（Stage B modeset 需實機） |

## 1. vc4 顯示管線（資料怎麼流）

```
framebuffer(記憶體) → HVS(合成/縮放，多 plane) → PixelValve(產生像素時序) → HDMI/DSI/DPI/VEC(輸出)
```
- **HVS**：把一或多個圖層（plane）合成、縮放成一張畫面。
- **PixelValve**：把合成結果轉成有時序的像素流（h/v sync）。
- **MOP/MOPLET**：BCM2712 新的輸出級。

## 2. 在 WDDM 怎麼對應

WDDM 的 modeset 路徑（`DxgkDdiCommitVidPn` 等）最終要：設好 HVS 的合成、PixelValve 的時序、
選定輸出（HDMI）。這對應 Linux 的 `atomic commit`。
> 概念雷同（都在描述「這張畫面怎麼合成、用什麼時序輸出」），但 API/物件模型完全不同，要照 WDDM 重寫。

## 3. 移植要點

- 先 DOD（單一 framebuffer、固定模式）就能點亮，不需完整 HVS 多 plane。
- 完整 modeset（解析度/多顯示器/plane）屬 Stage B，需實機調時序。

➡️ 回 [目錄](README.md)
