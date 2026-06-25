# Pi5 Linux GPU/媒體加速堆疊真相（2026-06-26 實機唯讀萃取）

> SSH 唯讀萃取 Pi5（kernel 6.x / Mesa 25.0.7），供 V3D/顯示移植參照。連線法見 `MD/Skill/pi5-ssh-hardware-facts.md`。
> 對應顯示硬體位址/暫存器見 `20260625-0200-pi5-display-facts.md`。

## 硬體
- GPU：BCM2712 **VideoCore VII**；3D core = **Broadcom V3D 7.1**（實測 `V3D 7.1.10.2`）。
- **render 與 display 分離**（兩個 DRM 節點）：
  - `card0 → v3d`（render-only，3D 運算）
  - `card1 → vc4-drm`（display/KMS：HVS 合成 + HDMI 掃描輸出）
- 載入模組：`v3d`、`vc4` + drm helpers。

## 使用者層 API（Mesa 25.0.7，Pi 官方 build `25.0.7-2+rpt4`）
| API | Linux 方案 | 實測 |
|-----|-----------|------|
| **Vulkan** | Mesa **`v3dv`**（`mesa-vulkan-drivers`）| ✅ `V3DV Mesa`，**Vulkan 1.3.305 conformant** |
| **OpenGL / GLES** | Mesa **`v3d` Gallium**（`libgl1-mesa-dri`）| GLES 3.1 / GL ~3.1 |
| **OpenCL/compute** | Rusticl（Mesa，選用）on v3d | — |
| 軟體 fallback | **llvmpipe**（CPU，Vulkan 1.4）| 有 |
- EGL/GBM on KMS；X11(glamor)/Wayland 都經此 GL/Vulkan。
- ⚠️ 舊封閉 **VideoCore GLES / Dispmanx / MMAL 已淘汰**；Pi5 全走開源 Mesa（Full KMS）。

## 媒體加速（與 3D 分開）
- **HEVC/H.265 硬體解碼**：`rpi-hevc-dec`（V4L2 stateless / request API；FFmpeg/GStreamer 用）。對應 source `drivers/media/.../hevc_dec`，detile 邏輯見 `windows_sources/hevc/sand.c`。
- **Pi5 無 H.264 硬解/硬編**（比 Pi4 砍掉）→ H.264 軟解（CPU）。
- `pispbe-*`（pispbe-config/input/output0/1/tdn/stitch…）= **PiSP Back End**（相機 ISP，M2M 影像處理），非顯示 GPU。

## 對 Windows (WDDM) 移植的意義 — GPU 3D 加速策略（精煉版）
目標（使用者定調）：**讓玩家在 Win11-ARM 有 3D 加速方案即可，Vulkan 或 OpenGL 皆可，D3D 其次**。

### ⚠️ Windows 關鍵現實：就算只要 Vulkan/OpenGL，也躲不掉 WDDM KMD
- **Linux**：Mesa(v3d/v3dv) **直接**對 DRM `v3d` render node 下 ioctl，無需其他東西。
- **Windows**：Vulkan/OpenGL 的 usermode ICD **一定**經 **D3DKMT → `dxgkrnl`（圖形核心子系統）→ WDDM KMD**，**無法**像 Linux 繞過去直打硬體。
- → **不論選哪個 API，底層都必須先有一個 WDDM 核心驅動（KMD）**。這是 GPU 移植**唯一的大工程、所有 API 共用的地基**：GPU MMU、CLE 指令提交（CT0/CT1）、中斷、VidMm/GpuMmu callbacks、排程、TDR。

### ✅ Vulkan 優先 → 一魚三吃（最省力，使用者方向正確）
Pi5 是 **Vulkan-first（v3dv 1.3 conformant）**，故只需做「**KMD + 一個 Vulkan UMD**」，其餘全疊上去：
```
WDDM KMD（port V3D 提交/MMU；所有 API 共用）        ← 唯一大工程
   +
Vulkan UMD = port Mesa v3dv（Windows ICD，走 D3DKMT；Mesa 已有 Windows winsys 基礎）
   ├─ Vulkan ...... 原生（Proton-style 遊戲）
   ├─ OpenGL ...... 用 Mesa Zink（GL-on-Vulkan）→ 不必另寫 GL 驅動
   └─ Direct3D（次要）... DXVK(D3D9/10/11→Vk) / vkd3d-proton(D3D12→Vk)  ← 「Vulkan wrap 成 D3D」
```
- **OpenGL 不必另寫**：Mesa **Zink** 跑在 Vulkan 上 → Vulkan 通則 GL 自動有。
- **D3D 不必原生寫**：DXVK / vkd3d-proton 即現成 wrapper（Steam Deck/Proton 同套路）。
- 投資集中在 **WDDM KMD + v3dv 的 D3DKMT winsys**；Vulkan/OpenGL/D3D 全解鎖。

### 其他
- 我們的 **DOD（display-only）** ＝ `vc4` 顯示路徑（HVS 掃描），與上面 3D 加速分開、不衝突。
- **HEVC 硬解**：`windows_sources/hevc/sand.c`（SAND→NV12）+ 在 `rpi-hevc-dec` 硬體上接 MFT/DXVA。
- H.264：Pi5 無硬體 → Windows 端也只能軟解（與 Linux 一致）。

## 待查證 / 後續
- v3dv 的 Vulkan 擴充清單（決定 Zink/DXVK/vkd3d 可達的 GL 版本 / D3D feature level）。
- Mesa Windows winsys（DRM ioctl ↔ D3DKMT 對應層）的移植面盤點。
- V3D KMD 在 Windows 的 GPU MMU / job submit（CLE CT0/CT1）對應（見 `MD/Note/gpu/` 藍圖）。
