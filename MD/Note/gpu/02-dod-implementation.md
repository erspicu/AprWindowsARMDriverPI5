# V3D GPU 移植：Phase 1 DOD 可落地實作（點亮畫面）

> 目標：HDMI 出現 Windows 桌面 + WARP 軟體渲染。**這是 ROI 最高、最該先做的**。承 [`01`](01-strategy-and-references.md)。

## 1. KMDOD 必實作的 DDI

**必做**：
- `DxgkDdiStartDevice`：map BCM2712 MMIO（HVS/HDMI…）、初始化結構。
- `DxgkDdiQueryChildRelations`：回報 1 個 video output target，狀態 Connected。
- `DxgkDdiQueryDeviceDescriptor`：給 EDID。
- **VidPn 三劍客**：`DdiIsSupportedVidPn` / `DdiRecommendFunctionalVidPn` / `DdiEnumVidPnCofuncModality`——**先 hardcode 只支援 1920×1080@60、`D3DDDIFMT_X8R8G8B8`**，不動態計算。
- `DxgkDdiCommitVidPn`：modeset 進入點。
- `DxgkDdiSetVidPnSourceAddress`：**最重要（flip）**——WARP 畫好 framebuffer 後叫你把實體位址給硬體掃描。
- `DxgkDdiInterruptRoutine` / `DpcRoutine`：VSync。

**可先 stub**：`SetPointerShape/Position`（回不支援 → WARP 畫軟體游標）、`SetPowerState`（回成功）、`SystemDisplayEnable/Write`（bugcheck 畫面，先跳過用 WinDbg）、I2C DDI（不必，內部讀 EDID）。

## 2. ★ 關鍵戰略：Hybrid（Mailbox 做 modeset，MMIO 做 flip）

> 在 Windows 還沒有 BCM2712 clock/power-domain driver 前，**直接敲 PixelValve/HDMI 暫存器 = 黑畫面地獄**。

**務實做法**：
1. **modeset 交給 VideoCore 韌體**（開機 bootloader 已點亮 HDMI）。`CommitVidPn` **不動 PV/HDMI 暫存器**，改用 **Mailbox property tags**：
   - `0x00048003` Set physical width/height
   - `0x00040001` Allocate buffer → 韌體回一個它配好的 framebuffer 實體位址
   - `0x00030020` Get EDID block（讓韌體走 I2C 讀 EDID，最安全）
2. **flip 用 MMIO**：`SetVidPnSourceAddress` 裡把新 framebuffer 實體位址動態寫進 HVS 的 display list（高效換頁，不靠慢的 mailbox）。

### BCM2712 顯示管線（若日後要純 MMIO modeset）
- **HVS**（合成）：讀 SRAM 裡的 **display list (dlist)** 合成；組 dlist（解析度/format XRGB8888/pitch/framebuffer 實體位址）寫進 `SCALER_DISPLISTx`。參考 `drivers/gpu/drm/vc4/vc4_hvs.c`（`vc4_hvs_update_dlist`）。
- **PixelValve**（時序）：H/V active/blank + pixel clock（要設 CPRMAN clock manager）。`vc4_crtc.c`。
- **HDMI/PHY**：TMDS、scrambler（>340MHz）、AVI InfoFrame。`vc4_hdmi.c` / `vc4_hdmi_phy.c`。
- **MOP/MOPLET**（Pi5 新增）：HVS 輸出可能要先 route 到 MOP 才進 PV/HDMI——**全新暫存器、需查 Linux vc4 / 實機**。

## 3. Framebuffer 配置與 scanout

- HVS DMA 只吃**連續實體記憶體**，但 OS surface 通常分散（paged）。
- **Phase 1 解法**：`StartDevice` 時用 `MmAllocateContiguousMemorySpecifyCacheNode` 預配 2-3 張 1080p 連續記憶體當 front/back buffer。`SetVidPnSourceAddress` 時 CPU `memcpy` OS surface → 連續記憶體 → 把實體位址寫 HVS。（CPU 負擔大但最穩；Phase 2 再做標準 GART/VRAM 管理。）

## 4. VSync

- BCM2712 顯示中斷 → ARM GIC；源在 PixelValve VBlank 暫存器。IRQ 號需查 ACPI/DSDT/實機。
- `DxgkDdiInterruptRoutine`：讀 PV 狀態 → 清 VSync flag → `KeInsertQueueDpc` → DPC 裡 `DxgkCbNotifyInterrupt(DXGK_INTERRUPT_CRTC_VSYNC)`。
- **無硬體 VSync 時 fallback**：`KeSetTimerEx` 每 16.6ms（60Hz）軟體報 VSync。

## 5. EDID
**Phase 1 不要寫 DDC/I2C**。hardcode 一個 1080p60 EDID byte array（128/256 bytes），`QueryDeviceDescriptor` 直接回傳；系統解析後 `IsSupportedVidPn` 就會要 1920×1080。要動態則用 mailbox `0x00030020`。

## 6. ★ 里程碑（切勿越級）

| M | 實作 | 驗收 |
|---|------|------|
| **M1 騙過 OS** | 註冊基本 DDI、回 hardcode EDID、`CommitVidPn` 直接 return success（不碰硬體）| RDP/WinDbg 連入，顯示設定看到「一般 PnP 顯示器」1080p |
| **M2 軟體 VSync + WARP** | `KeSetTimerEx` 16.6ms 報 VSync | 系統不卡，WARP 開始把桌面丟給 `SetVidPnSourceAddress` |
| **M3 UEFI GOP 劫持 🚀第一個像素** | `SetVidPnSourceAddress` 裡 `memcpy` WARP surface → **bootloader 留下的 UEFI GOP framebuffer 實體位址**（轉圈圈那塊）| **轉圈圈消失、出現 Windows 桌面！** 滑鼠會動（memcpy 慢，<10 FPS）|
| **M4 HVS 硬體 flip** | 停止 memcpy，改預配連續記憶體 + 寫 HVS display list 觸發更新 | 畫面流暢、CPU 降載（DOD 最終型態）|

> **M3 是最快「看到桌面」的捷徑**——劫持已經被韌體點亮的 GOP framebuffer，連 BCM2712 顯示暫存器都不必碰。

## 行動清單
1. 補 `rp1vc4dod.sys` 的 DDI 過 M1。
2. 查 Pi mailbox 文件，準備用 mailbox 讀 EDID。
3. WinDbg 攔 `SetVidPnSourceAddress` 確認 OS 在給 framebuffer。
4. 看 Linux 先專注 `vc4_hvs.c` 的 dlist 組合/寫入，**先別碰** `vc4_crtc.c`/`vc4_hdmi.c`（太龐雜）。
