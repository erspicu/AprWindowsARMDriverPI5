# 顯示輸出：多 head（VidPN）+ 各輸出實作

> 承 [`README`](README.md)。建立在 [`../gpu/02-dod-implementation.md`](../gpu/02-dod-implementation.md) 的 HDMI0 DOD 之上。
> 暫存器一律標「需查 Linux vc4 source」。

## 1. BCM2712 顯示管線（與舊 Pi 不同）

- 舊 HVS 概念在 BCM2712 演進為 **MOP（Main Output Pipeline）+ MOPLET（較小 pipeline）**。
- HDMI0 ← MOP、HDMI1 ← MOPLET，各有獨立 PixelValve(timing controller) + PHY，**可獨立 modeset**（共用記憶體頻寬）。
- MIPI-DSI：Pi5 板載 2 個 MIPI 收發器（可設 DSI 或 CSI），由另一個獨立 CRTC 餵。
- VEC：BCM2712 內部 VEC(video encoder) IP。
- Linux 參考：`drivers/gpu/drm/vc4/` 的 `vc4_hdmi.c`/`vc4_dsi.c`/`vc4_vec.c`；Pi5 的 MOP/MOPLET 是新引入。

## 2. WDDM 多 head（VidPN）

- DOD 實作 **`DxgkDdiQueryChildRelations`**：回報所有實體連接埠陣列，每個是一個 child（`ChildUid`，如 HDMI0=1、HDMI1=2、DSI=3）。
- `DxgkDdiIsSupportedVidPn` / `DxgkDdiEnumVidPnCofuncModality`：定義哪個 source（pipeline/CRTC）可連到哪個 target（ChildUid）。
- **HDMI×2 同時**：VidPN 建兩條 path（`Source0→Target1(HDMI0)`、`Source1→Target2(HDMI1)`）；DWM 自動處理延伸/同步桌面。
- 參考：WDK `KMDOD` sample（含軟體模擬多螢幕邏輯）。

## 3. 各輸出實作差異

| 輸出 | EDID | timing | 點亮要做 | 額外工（在 HDMI0 之上）|
|------|:---:|--------|----------|:---:|
| **HDMI1** | 讀 | 動態 | 同 HDMI0，暫存器配置多傳 index(0/1)；VidPN 認兩個 target | **+10-20%** |
| **MIPI-DSI** | 無 | 固定 | DSI PHY 初始化 + **DSI host controller command R/W** + 面板 **MIPI DCS init 序列**（wake/set pixel format/display on）→ 切 video mode 送像素 | **+100-150%** |
| **VEC** | 無 | 固定 PAL576i/NTSC480i | 寫一堆 magic-number 類比暫存器啟動（無 I2C/EDID）| **+40%** |
| **DPI** | — | — | **Pi5 放棄**（GPIO 在 RP1，像素無法經 PCIe）| — |

### DSI 面板初始化（關鍵）
- DSI **無 EDID**，面板有固定 timing（resolution/porch/sync）。
- 在 DOD modeset 流程裡 **hardcode 面板 init sequence**（透過 DSI command mode 打一串 hex），再切 video mode。
- 背光：獨立 PWM（可能在 RP1）或 I2C 指令，可寫死或另做小工具。
- 參考：`vc4_dsi.c`（host 邏輯）+ `panel-raspberrypi-touchscreen.c`（官方 7" 面板 init sequence）。

## 4. 觸控（與顯示獨立）

- 官方 7" DSI 螢幕：影像 MIPI-DSI、觸控 **I2C（FT5406）+ INT pin**，**兩條獨立路徑**。
- Windows：ACPI DSDT 把觸控 I2C 晶片宣告成 **`PNP0C50`（I2C HID）** → inbox `hidi2c.sys` + `mshidkmdf.sys` 自動接管 → 隨插即用 10 點觸控。
- **需驗證**：FT5406 韌體是否完全相容 HID over I2C；早期官方觸控屏有自定義 register map → 不相容則寫 KMDF/UMDF **HID filter** 把 raw I2C 轉成 HID report descriptor（屬 HID 範疇，與 DOD 無關）。
- 走已移植的 **RP1 I2C**。

## 5. 落地順序
HDMI0（[gpu DOD](../gpu/02-dod-implementation.md)）→ **HDMI1（雙螢幕，最划算）** → DSI（觸控屏需求）→ VEC（復古）。DPI 放棄。
