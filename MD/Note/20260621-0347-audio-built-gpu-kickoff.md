# 音效 Phase 2 建置完成 + GPU/顯示驅動啟動

> 日期：2026-06-21 03:47（loop job 28957e8c）

## 音效（PortCls/WaveRT）— 可建置里程碑達成 ✅
- 產出：`windows_driver/audio/rp1i2saud.sys`（ARM64, 36 KB, Native, entry `GsDriverEntry`）
- 驗證：import `portcls.sys`（`PcNewPort`/`PcRegisterSubdevice`/`PcInitializeAdapterDriver`）+ `ntoskrnl.exe`
- 原始碼：`windows_sources/audio/rp1-i2s-portcls/`（adapter + minwave + minwavestream + mintopo + tables，重用 Phase 1 的 I2S HAL）

### 本輪新增 know-how（link 階段踩雷）
1. `MAXULONGLONG` 在 kernel 標頭未定義 → 用 `PHYSICAL_ADDRESS.QuadPart = -1`（全 1 = 不限高位址）。
2. `stdunk.lib` 的 `CUnknown::NonDelegatingAddRef/Release` 參考 `_InterlockedIncrement/_Decrement` → 需連結 **`libcntpr.lib`**（kernel C runtime；用 `dumpbin /linkermember` 掃 km\arm64 找到的）。
3. C++ PortCls link 庫集合（已驗證可連結）：`portcls.lib stdunk.lib ksguid.lib libcntpr.lib BufferOverflowFastFailK.lib ntoskrnl.lib hal.lib`，entry `GsDriverEntry`。

### 音效「完成」的誠實界定（無實機天花板）
此為**可建置、可載入、會註冊 render endpoint 結構**的驅動骨架；但**尚不能真的出聲**，因為缺：
- `DataRangeIntersection` 實作（目前回 `STATUS_NOT_IMPLEMENTED`）
- 真實 DMA 資料路徑（RP1 DesignWare AXI-DMA）
- codec/時脈、HAL 接真實 MMIO（裝置在 RP1→PCIe 底下，Windows 尚未列舉）
這些都需**實機**才能有意義地實作與驗證。故音效在「無硬體」前提下視為完成，依使用者指示**轉進 GPU/顯示**。

---

## GPU / 顯示驅動 — 啟動（最高難度）

> 使用者指示：音效完成後繼續挑戰 GPU 與顯示驅動。

### 硬體（取自 RPi5-Driver-Porting-Inventory）
- **顯示**：BCM2712 HDMI0/1（`vc4_hdmi`）、HVS 影像合成（`vc4_hvs`）、PixelValve、MOP/MOPLET → vc4 DRM 顯示管線；另 RP1 有 DSI/DPI/VEC。
- **GPU**：V3D（VideoCore VII，`drivers/gpu/drm/v3d`）。

### Windows 對應：WDDM（最複雜的驅動模型）
WDDM = **KMD**（kernel display miniport，掛 `dxgkrnl`）+ **UMD**（user-mode D3D）。完整 WDDM+3D 工程量等同重寫一個 GPU 驅動，且無實機無法驗證。

### 增量路線（務實，逐 fire 推進）
1. **Stage A — Display-Only Driver (DOD)**：WDDM KMD only，先點亮 HDMI framebuffer。對應 WDK `kmdod` 範例，掛 `DxgkInitializeDisplayOnlyDriver`，實作最小 DDI。**這是第一個可建置目標**。
2. **Stage B — KMS modeset**：把 vc4 的 HVS + PixelValve + HDMI 模式設定序列移植進 DOD。
3. **Stage C — V3D GPU 加速**：完整 WDDM render（command submission、GPU VA、排程）。最難，最後做。

### 專案位置
- `windows_sources/display/rp1-vc4-dod/`（Stage A 起點，下一個 loop fire 開始 scaffold）
- 建置沿用手動 cl/link 模式；DOD 需連結 dxgkrnl 的 DDI lib（待查：`dxgkrnl.lib`/`displib`），下次用 `tools/knowledgebase` 問 Gemini + 讀 WDK `dispmprt.h`/`d3dkmddi.h` 取得精確 DDI。

### 解除 loop 條件（更新）
音效已達無硬體天花板；GPU/顯示同樣會在「無實機無法再進展」時停。屆時 `CronDelete 28957e8c` + PushNotification。
