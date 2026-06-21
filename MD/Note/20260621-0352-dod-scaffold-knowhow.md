# GPU/顯示 Stage A — Display-Only Driver (DOD) scaffold know-how

> 日期：2026-06-21 03:52（loop job 28957e8c）
> 狀態：DriverEntry + init data **已編譯（ARM64）**；callback 實作 + link 待下個 fire。

## 專案
- `windows_sources/display/rp1-vc4-dod/`（`common.h`, `driver.c`, `build.ps1`）

## 已確認的 DOD know-how（用 WDK 標頭 + dumpbin 驗證）
1. **DriverEntry 用 `DxgkInitializeDisplayOnlyDriver`**（不是一般 WDM），init 結構是 **`KMDDOD_INITIALIZATION_DATA`**（非 `DRIVER_INITIALIZATION_DATA`）。標頭：`km\dispmprt.h`。
2. **版本**：`init.Version = DXGKDDI_INTERFACE_VERSION`（在 26100 解析為 `DXGKDDI_INTERFACE_VERSION_WDDM3_2`）。
3. **Link lib = `displib.lib`**（用 `dumpbin /linkermember` 掃 km\arm64 確認 `DxgkInitializeDisplayOnlyDriver` 在此）。⚠️ **`dxgkrnl.lib` 在 km\arm64 不存在**（Gemini 誤稱要 dxgkrnl.lib；以 dumpbin 為準）。完整 link：`displib.lib libcntpr.lib BufferOverflowFastFailK.lib ntoskrnl.lib hal.lib`，entry `GsDriverEntry`。
4. **精確 callback 簽章技巧**：用 WDK 的**函式 typedef**（`DXGKDDI_ADD_DEVICE` 等，定義於 `shared\d3dkmddi.h`，含 `_Function_class_DXGK_`）做前向宣告：`DXGKDDI_ADD_DEVICE DodAddDevice;`，簽章由標頭鎖定（同 Phase 1 的 `DRIVER_DISPATCH` 手法）。
5. **必填 callback（19 個，缺則 `DxgkInitializeDisplayOnlyDriver` 失敗）**：AddDevice, StartDevice, StopDevice, RemoveDevice, QueryChildRelations, QueryChildStatus, QueryDeviceDescriptor, SetPowerState, QueryAdapterInfo, IsSupportedVidPn, RecommendFunctionalVidPn, EnumVidPnCofuncModality, SetVidPnSourceVisibility, CommitVidPn, UpdateActiveVidPnPresentPath, RecommendMonitorModes, QueryVidPnHWCapability, PresentDisplayOnly, StopDeviceAndReleasePostDisplayOwnership。
6. **可留 NULL（最小 bring-up）**：DispatchIoRequest, InterruptRoutine, DpcRoutine, SetPointerPosition, SetPointerShape, SystemDisplayEnable/Write。
7. include 順序同前：`km\crt`→`km`→`shared`；`#include <ntddk.h>` 後 `#include <dispmprt.h>`。

## 待辦（下個 fire）
- [ ] `ddi.c`：實作 19 個 callback（先 stub：能載入、VidPn 用最小合法回應）。先從 `d3dkmddi.h` 抽出每個 `DXGKDDI_*(...)` 的精確參數。
- [ ] link → `rp1vc4dod.sys`，交付 `windows_driver/display/`
- [ ] INF（Display class `{4d36e968-...}`）
- [ ] Stage B：把 vc4 的 HVS/PixelValve/HDMI modeset 接進 VidPn/Present 路徑（需實機才有意義）

## 參考
- 啟動路線：`MD/Note/20260621-0347-audio-built-gpu-kickoff.md`
- Gemini 問答：`temp/gemini_a3.txt`
