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

## 對 Windows (WDDM) 移植的意義
- 我們的 **DOD（display-only）** ＝ `vc4` 顯示路徑（HVS 掃描）；**不含 3D 加速**。
- **3D 加速路線（關鍵策略）**：Pi5 是 **Vulkan-first（v3dv 為一等公民、1.3 conformant）**。故務實路線 =
  **port Mesa `v3dv` → 在其上疊 DXVK（D3D9/10/11→Vulkan）/ vkd3d-proton（D3D12→Vulkan）** ＝ 比從零寫 D3D UMD 省非常多。KMD 仍需 V3D 記憶體/CLE/中斷（暫存器見 `20260625-0200` V3D 段）。
- **HEVC 硬解**：`windows_sources/hevc/sand.c`（SAND→NV12）+ 在 `rpi-hevc-dec` 硬體上接 MFT/DXVA。
- H.264：Pi5 無硬體 → Windows 端也只能軟解（與 Linux 一致）。

## 待查證 / 後續
- v3dv 的 Vulkan 擴充清單（決定 DXVK/vkd3d 可達的 D3D feature level）。
- V3D KMD 在 Windows 的 GPU MMU / job submit（CLE CT0/CT1）對應（見 `MD/Note/gpu/` 藍圖）。
