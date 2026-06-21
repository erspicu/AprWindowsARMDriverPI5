# DOD 建置完成 + GPU(V3D) 階段啟動

> 日期：2026-06-21 03:58（loop job 28957e8c）

## 顯示 Stage A — Display-Only Driver 可建置 ✅
- 產出：`windows_driver/display/rp1vc4dod.sys`（ARM64, Native, `GsDriverEntry`, ~29 KB）
- 原始碼：`windows_sources/display/rp1-vc4-dod/`（`common.h`, `driver.c`, `ddi.c`, `build.ps1`）
- 19 個必填 DXGK DDI 全部實作為最小 stub；用 WDK 函式 typedef + `IN_/OUT_` 巨集鎖定簽章；link `displib.lib libcntpr.lib BufferOverflowFastFailK.lib ntoskrnl.lib hal.lib`。

### 本輪 know-how
- DOD callback 參數用 WDK 巨集（`IN_CONST_PVOID`/`OUT_PULONG`/`IN_CONST_HANDLE`…）；d3dkmddi.h 的 DDI 回傳型別是 `NTSTATUS APIENTRY`，dispmprt.h 的是純 `NTSTATUS`。
- `QueryDeviceDescriptor` 回 `STATUS_MONITOR_NO_DESCRIPTOR` 讓 dxgk 合成預設模式；`RecommendFunctionalVidPn` 回 `STATUS_GRAPHICS_NO_RECOMMENDED_FUNCTIONAL_VIDPN`。
- `ExAllocatePool2(POOL_FLAG_NON_PAGED, …)` 配置 context（自動歸零）。

### 顯示的無硬體天花板
DOD 可建置/可載入結構，但**真正點亮畫面**需 **Stage B**：在 `CommitVidPn`/`PresentDisplayOnly` 裡程式化 vc4 的 **HVS/PixelValve/HDMI** 暫存器並提供 framebuffer——這需**實機**（裝置經 RP1/BCM2712 顯示 HW 列舉）才有意義。故顯示在無硬體前提下視為達成天花板。

---

## GPU 本體 — V3D（VideoCore VII）WDDM render（最後挑戰）
- Linux 來源：`drivers/gpu/drm/v3d`。
- Windows 對應：**完整 WDDM render KMD**（非 display-only）+ UMD（user-mode D3D）。比 DOD 大得多：需 GPU node 排程、command submission、GPU VA/分頁、allocation 管理（DxgkDdiCreateDevice/CreateAllocation/Render/Patch/SubmitCommand/BuildPagingBuffer…）。
- **完全需要實機**才能驗證；本 loop 只能做「可建置骨架」。
- 專案：`windows_sources/gpu/rp1-v3d/`（下個 fire 起 scaffold；先用 Gemini + WDK `d3dkmddi.h` 取得 render KMD 的 `DRIVER_INITIALIZATION_DATA`（注意：render 用 `DriverEntry`+`DxgkInitialize`，結構是 `DRIVER_INITIALIZATION_DATA`，與 DOD 的 `KMDDOD_INITIALIZATION_DATA` 不同）。

## 整體進度與天花板
| 驅動 | 狀態 | 產出 |
|------|------|------|
| 音訊 WDM 骨架 | ✅ build | `rp1i2s.sys` |
| 音訊 PortCls/WaveRT | ✅ build | `rp1i2saud.sys` |
| 顯示 WDDM DOD | ✅ build | `rp1vc4dod.sys` |
| GPU V3D WDDM render | ⏳ 下一個 | `rp1-v3d/` |

> 三類驅動皆已達「無實機可建置骨架」；GPU V3D 為最後一個可做的骨架。完成後即抵達「無實機無法再進展」，屆時解除 loop（`CronDelete 28957e8c` + PushNotification）。
