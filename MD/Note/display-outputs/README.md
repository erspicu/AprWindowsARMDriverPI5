# 顯示輸出（HDMI×2 / DSI / VEC / DPI）移植筆記

> Pi5（BCM2712）**主 HDMI 以外的顯示輸出 + 多螢幕**在 Windows on ARM64 的處理。
> 主 HDMI 點亮（HVS→PixelValve→HDMI、Hybrid mailbox+MMIO、UEFI GOP 劫持）見 [`../gpu/`](../gpu/)（DOD 筆記）。
> 來源：本專案分析 + Gemini 1 輪諮詢。建立：2026-06-22

## Pi5 顯示輸出全貌

| 輸出 | 來源 | 特性 | 優先序 |
|------|------|------|:---:|
| **HDMI0** | BCM2712 MOP | 標準（EDID/TMDS/FRL/InfoFrame）| ① 必做（涵蓋 95%）|
| **HDMI1** | BCM2712 MOPLET | 同 HDMI0 IP（只是 base/IRQ 不同）| ② **強烈建議**（+10-20%，達成雙螢幕）|
| **MIPI-DSI**（官方 7" 觸控屏）| 獨立 CRTC | **無 EDID**、固定 timing、要 **MIPI DCS 面板初始化序列** | ③ niche（+100-150%）|
| **VEC**（composite 類比）| BCM2712 VEC IP | 最死板，固定 PAL576i/NTSC480i，無 EDID/I2C | ④ 極 niche（+40%）|
| **DPI**（並行 RGB）| — | **Pi5 已淘汰**（GPIO 移到 RP1，無法經 PCIe 推像素）→ 用 DSI-to-DPI bridge IC | ⛔ 放棄 |

> 都騎在同一套 **DOD/WDDM** 上；多輸出靠 **VidPN**（`DxgkDdiQueryChildRelations` 回報多個 child target）。

## 務實建議
- **HDMI0 做完即涵蓋 95%**；**HDMI1 是最划算的第二步**（IP 幾乎相同，達成雙螢幕，對「桌面替代品」展示意義大）。
- DSI/VEC 屬 niche（觸控屏改裝／復古玩家），DPI 直接放棄。

## 觸控（官方 DSI 螢幕）是**獨立路徑**
影像走 MIPI-DSI；觸控走 **I2C（FT5406）+ INT pin**，與 DOD 無關。Windows 做法：ACPI DSDT 把觸控晶片宣告成
**PNP0C50（I2C HID）**→ inbox `hidi2c.sys`/`mshidkmdf.sys` 自動接管成 10 點觸控（前提：韌體符合 HID over I2C；
不符則寫 KMDF/UMDF HID filter 轉 report）。走已移植的 RP1 I2C。

## 筆記索引
| 檔案 | 內容 |
|------|------|
| [`01-multihead-and-outputs.md`](01-multihead-and-outputs.md) | VidPN 多 head 表達、各輸出（HDMI1/DSI/VEC）實作差異與額外工、DSI 面板初始化、觸控、參考 |

## 與 GPU 筆記的關係
本筆記是 [`../gpu/02-dod-implementation.md`](../gpu/02-dod-implementation.md)（HDMI DOD 點亮）的延伸：先把 HDMI0 點亮，再依優先序加 HDMI1 → DSI → VEC。
