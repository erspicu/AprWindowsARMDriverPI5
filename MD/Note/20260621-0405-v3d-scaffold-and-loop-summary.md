# V3D GPU scaffold + 移植 loop 收尾總結

> 日期：2026-06-21 04:05（loop job 28957e8c — **本筆記後解除**）

## GPU V3D — WDDM render KMD scaffold ✅（可編譯）
- 原始碼：`windows_sources/gpu/rp1-v3d/`（`common.h`, `driver.c`, `build.ps1`）
- `DriverEntry` 透過 **`DxgkInitialize`** + `DRIVER_INITIALIZATION_DATA` 註冊 38 個 render/VidPn DDI（用 WDK 函式 typedef 鎖定簽章）。**compile-check 通過（ARM64）**。
- **Link lib = `displib.lib`**（`dumpbin /linkermember` 實證 `DxgkInitialize` 與 `DxgkInitializeDisplayOnlyDriver` 都在 displib.lib；km\arm64 **無** dxgkrnl.lib）。⚠️ Gemini 兩次都誤稱要 dxgkrnl.lib — **以 dumpbin 實證為準**。
- cl 旗標補充（Gemini）：render KMD 建議 `/DNTDDI_VERSION=0x0A00000F`（WDDM3.2 結構）、`/arch:armv8.0`。

### render KMD 必填 DDI（下一步寫 stub→link 用）
PnP/power：AddDevice, StartDevice, StopDevice, RemoveDevice, Unload, SetPowerState, QueryAdapterInfo；I/O：DispatchIoRequest, Escape；中斷：InterruptRoutine, DpcRoutine；topology：QueryChildRelations/Status/DeviceDescriptor；記憶體：CreateAllocation, DestroyAllocation, DescribeAllocation, GetStandardAllocationDriverData, OpenAllocation, CloseAllocation, BuildPagingBuffer；device/context：CreateDevice, DestroyDevice, CreateContext, DestroyContext；執行：Patch, Render, Present, ResetFromTimeout, RestartFromTimeout；VidPn：IsSupportedVidPn, RecommendFunctionalVidPn, EnumVidPnCofuncModality, SetVidPnSourceAddress, SetVidPnSourceVisibility, CommitVidPn, UpdateActiveVidPnPresentPath。

---

## 全專案產出總結（無實機可建置天花板已達）
| 驅動 | 模型 | 狀態 | 產出 |
|------|------|------|------|
| 音訊 I2S 硬體層 | WDM + HAL | ✅ build | `windows_driver/audio/rp1i2s.sys` |
| 音訊 sound card | PortCls/WaveRT | ✅ build | `windows_driver/audio/rp1i2saud.sys` |
| 顯示 | WDDM Display-Only | ✅ build | `windows_driver/display/rp1vc4dod.sys` |
| GPU V3D | WDDM render | ✅ 可編譯 scaffold | `windows_sources/gpu/rp1-v3d/` |

工具鏈：x64→ARM64 交叉編譯，WDK 10.0.26100 + VS2022 MSVC ARM64，手動 cl/link（winget WDK 無 MSBuild Driver 整合）。

## 為何在此解除 loop（無實機天花板）
所有「下一步」都**必須有實體 Raspberry Pi 5 + Windows-on-Pi5 開機環境**才能進行/驗證：
1. **RP1 在 PCIe 底下**：Windows 要先打通 BCM2712 PCIe 列舉 + RP1 multi-function bus，並由 **ACPI** 描述各裝置 MMIO/中斷（DT→ASL）。沒有它，音訊/I2C/SPI/USB 全部看不到。
2. **音訊**：DataRangeIntersection、DMA 資料路徑、codec/時脈、HAL 接真實 MMIO — 需實機。
3. **顯示**：vc4 HVS/PixelValve/HDMI 的 modeset 與 framebuffer present — 需實機。
4. **GPU**：完整 render DDI stub→link 是機械工，但**任何實際功能**需 V3D 硬體 + command submission（移植 Linux v3d 的 CL/bin/render 提交）+ GPU VA/分頁 + **UMD（user-mode D3D 驅動）**；ARM64 弱記憶體序下 allocation cache flag 錯誤會 bugcheck 0x119。

→ 已到「無實機無法再進展」之點（CLAUDE.md 既定的解除條件）。

## 如何恢復（有實機後）
- 重啟 loop：`/loop 5m <prompt>` 或直接指定下一個 track。
- 各 track 下一步：見本目錄 `20260621-0331`(音訊P1)、`0341`(音訊P2)、`0347/0352/0358`(顯示)、本篇(GPU)。
- 先決條件 track（最關鍵）：**BCM2712 PCIe RC + RP1 列舉 + ACPI 描述**（`pcie-brcmstb` → Windows PCI/ACPI），這是一切 I/O 的命脈。
