# V3D GPU 移植：策略 + WDDM 架構 + 參考

## 1. WDDM 架構：點亮 vs 加速「完全解耦」

| | DOD (Display-Only Driver) | Render Driver (Full WDDM) |
|---|---|---|
| 負責 | 只管 display controller（Pi 的 HVS/PixelValve/HDMI）| 3D 引擎（V3D 7.1）|
| 核心 DDI | `DdiCommitVidPn`(modeset)、`DdiSetVidPnSourceAddress`(flip) | KMD（排程/記憶體）+ UMD（D3D/GL/Vulkan）|
| 繪圖 | **WARP（CPU 軟體渲染）**畫好 framebuffer，DOD 只 scanout | GPU 硬體渲染 |

### Render KMD（Dxgkrnl miniport）最小必做 DDI
- 裝置：`DxgkDdiAddDevice` / `StartDevice`
- **Context & 排程（最難）**：`CreateContext` / `SubmitCommand`（收 UMD 的 command buffer 給 GPU）/ `PreemptCommand`（WDDM 1.2+ 強制）
- **記憶體（極難）**：`CreateAllocation` / `BuildPagingBuffer`（建 GPU 頁表 / MMU 映射）——VidMm 與 Linux DRM 差異極大
- 同步：`SignalMonitoredFence`

### Render UMD（user-mode）最小做什麼
- 攔 API（D3D/Vulkan）→ **shader compiler**（HLSL/SPIR-V → Broadcom QPU 機器碼）→ **CL generation**（state → V3D Control List）→ `D3DKMTSubmitCommand` 丟給 KMD。

## 2. 唯一務實槓桿：重用 Mesa

> 自己寫 V3D shader compiler + CL generator = **人年級災難**。Mesa 已有 `v3d`(Gallium GL) + `v3dv`(Vulkan)，
> 支援 V3D 7.1。把 Mesa 當「**shader 編譯器 + command buffer 產生器**」，你只寫 **Windows Winsys**（把 Linux DRM
> ioctl 換成 WDDM D3DKMT）+ 一個 KMD。

兩種 UMD 接法：
- **(a) Gallium `v3d` + `d3d10umd` state tracker**：最適合「原生桌面加速」（DWM 要 D3D11）。但要寫原生 D3D 路徑。
- **(b) `v3dv`(Vulkan ICD) + DXVK/VKD3D**：最適合**遊戲**——遊戲 D3D → DXVK → Vulkan → v3dv。**玩家路線**（見 [04](04-full-acceleration-and-knowledge-boundary.md)）。
- (c) Dozen/DZN：✗ 方向相反（它是 Vulkan-on-D3D12）。

## 3. 工程量 / 風險

| Phase | 工程量（1-2 資深工程師）|
|-------|------|
| 1 DOD 完善 | 1-2 月 |
| 2 Mesa winsys + 基礎 KMD IOCTL | 3-5 月 |
| 3 完整 WDDM 整合（VidMm/TDR/DXVK）| 6-12 月 |
| **總計** | **~1-1.5 人年** |

**最大風險**：① WDDM VidMm vs V3D MMU 衝突（`BuildPagingBuffer` → V3D page table，最大 BSOD/TDR 來源）② TDR（GPU hang → reset，沒處理好 BSOD 0x119）③ Pi 韌體/電源管理（DVFS 靠 VideoCore FW，Windows 端缺文件）。

## 4. 開源參考 / SOURCES

| 資源 | 用途 | 連結 |
|------|------|------|
| **WDDM Design Guide**（MS Learn）| VidMm + DxgkDdi 介面 | learn.microsoft.com → "Windows Display Driver Model (WDDM)" |
| **KMDOD sample**（寫 DOD 聖經）| Display-only driver 範本 | github.com/microsoft/Windows-driver-samples → `video/Kmdod` |
| **Red Hat viogpu**（virtio-gpu WDDM）⭐ | **最佳 DOD + 基礎 Render KMD 架構參考**（`BuildPagingBuffer`/command submission）| github.com/virtio-win/kvm-guest-drivers-windows → `viogpu` |
| **WoR Pi4 wddm**（driver1998）| Pi4 DOD（Broadcom mailbox/firmware/framebuffer）| github.com/driver1998/bcm271x-wddm |
| **Mesa** ⭐⭐ | V3D 官方驅動：`src/gallium/drivers/v3d/`、Vulkan `src/broadcom/vulkan/`(v3dv)、編譯器 `src/broadcom/compiler/`、Linux winsys `src/gallium/winsys/v3d/drm/`、D3D10 封裝 `src/gallium/targets/d3d10umd/` | gitlab.freedesktop.org/mesa/mesa |
| **DXVK / VKD3D-proton** | D3D→Vulkan（玩家路線）| github.com/doitsujin/dxvk |

➡️ DOD 實作見 [`02`](02-dod-implementation.md)；加速 Mesa 路線見 [`03`](03-acceleration-mesa-path.md)；完整加速 + 知識邊界見 [`04`](04-full-acceleration-and-knowledge-boundary.md)。
