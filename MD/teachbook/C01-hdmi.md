# C1：HDMI ×2（vc4，WDDM）

> 多媒體第一章。顯示/GPU 是整個移植**工程量最大**的一塊——Windows 要的是完整 **WDDM**，與 Linux DRM/KMS 概念差很多。

| | |
|---|---|
| **裝置** | BCM2712 HDMI ×2 |
| **Linux 源碼** | `drivers/gpu/drm/vc4/vc4_hdmi.c` |
| **Windows 框架** | **WDDM** display miniport |
| **本專案** | 規劃中；先做 Display-Only Driver（DOD）點亮畫面 |

## 1. 顯示移植的三階段策略

```
Stage A：Display-Only Driver (DOD)   先點亮 HDMI、給一個 framebuffer（kmdod 式，最小可用）
Stage B：KMS modeset                 vc4 的 HVS/PixelValve/HDMI 完整 modeset（解析度切換）
Stage C：V3D 加速                    完整 GPU render（見 C3，工程量最大）
```
> 務實做法：**先 DOD 點亮畫面**（有東西看），再逐步往上。本專案 DOD 骨架已 build（`rp1vc4dod.sys`，19 DDI stub，link `displib.lib`）。

## 2. HDMI 本身要做什麼

- PHY/序列器設定、TMDS 時脈。
- 讀 EDID（透過 DDC I2C）決定支援的解析度。
- 與 [HVS（C2）](C02-hvs-display.md) 的顯示管線串接。

## 3. 為什麼這塊最難

- Windows 沒有「直接對應 vc4」的東西——WDDM 是一整套 DDI 合約（modeset、framebuffer、present、電源…）。
- HDMI 不能單獨運作，要連同 HVS + PixelValve 整條顯示管線。
- 屬 Tier 3，建議在基本系統（PCIe/SD/Ethernet/USB）穩定後再投入。

➡️ 回 [目錄](README.md)
