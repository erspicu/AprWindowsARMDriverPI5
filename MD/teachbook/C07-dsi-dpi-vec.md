# C7：RP1 顯示輸出 — DSI / DPI / VEC（WDDM）

> RP1 上的三種**替代顯示輸出**（非 HDMI）：MIPI DSI 面板、DPI 平行顯示、VEC 類比電視。

| | |
|---|---|
| **裝置** | RP1 MIPI DSI ×2、DPI、VEC（類比電視） |
| **Linux 源碼** | `drivers/gpu/drm/rp1/rp1-dsi/`、`rp1-dpi/`、`rp1-vec/` |
| **Windows 框架** | **WDDM**（額外的輸出 connector） |
| **本專案** | 規劃中（🔲，Tier 3） |

## 1. 三種輸出

| 輸出 | 用途 |
|------|------|
| **DSI** | MIPI DSI 面板（如官方觸控螢幕） |
| **DPI** | 平行 RGB 顯示（老式 LCD/排針） |
| **VEC** | 類比電視（composite，CVBS） |

## 2. 在顯示管線的位置

和 [HDMI（C1）](C01-hdmi.md) 一樣，都是「[HVS（C2）](C02-hvs-display.md) → PixelValve → 輸出級」的**輸出端**之一。
差別只在最後的物理介面（DSI 序列器 / DPI 平行 / VEC DAC）。

## 3. 移植要點

- 在 WDDM 裡，這些是不同的 **target / connector**；modeset 時選對輸出。
- 優先做 HDMI（最常用）；DSI/DPI/VEC 視需求補。
- 屬 Tier 3，需實機。

➡️ 回 [目錄](README.md)
