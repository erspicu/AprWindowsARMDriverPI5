# Skill：建置工具鏈（x64 → ARM64 交叉編譯）

> 操作型 how-to。基本守則見 `../../CLAUDE.md`。

## 環境
- x64 主機**交叉編譯**至 ARM64：**WDK 10.0.26100 + VS2022「MSVC v143 ARM64 build tools」**。
- winget 版 WDK **不含** VS 的 MSBuild Driver 整合（無 `WDK.vsix`）→ 故**不走 .vcxproj**，
  各驅動以 `windows_sources/<類別>/<專案>/build.ps1`（**直接呼叫 `cl`/`link`**）建置。

## include 順序（重要）
`wdf\kmdf\1.33` → `<SDK>\km\crt` → `<SDK>\km` → `<SDK>\shared`
**勿加 VC user-mode include**（kernel 與 user-mode 標頭會衝突）。

## 連結配方（依框架）
| 驅動型 | entry | 關鍵 lib |
|--------|-------|----------|
| KMDF | `FxDriverEntry` | `wdfdriverentry.lib` + `wdfldr.lib` |
| 非 KMDF (WDM/miniport) | `GsDriverEntry` | — |
| GpioClx | — | `msgpioclxstub.lib` |
| SdPort | — | `sdport.lib` |
| NDIS | — | `ndis.lib` |
| PortCls | — | `portcls.lib` + `stdunk.lib` + `ksguid.lib` |
| AVStream | — | `ks.lib` + `ksguid.lib`（**須以 C++ 編譯**，DriverEntry 包 `extern "C"`）|
| WDDM | — | `displib.lib` |

共通 link：`/DRIVER /SUBSYSTEM:NATIVE,10.0 /MACHINE:ARM64 /NODEFAULTLIB /INTEGRITYCHECK`
+ `libcntpr.lib BufferOverflowFastFailK.lib ntoskrnl.lib hal.lib`。

## x64 模擬驗證（sim pattern）
HAL 抽暫存器狀態機（純 `void* Base`）→ regio shim（真機 `READ/WRITE_REGISTER`；sim 用 `<X>_SIM` mock）
→ `sim/<dev>_simshim.h`「fake kernel」→ `sim/<dev>_sim.c`（mock 暫存器 + 斷言）。

```powershell
# 不需硬體，純 x64 user-mode 跑
cl /D<X>_SIM /I.. sim\<dev>_sim.c ..\<dev>_hw.c /Fe:<out>.exe
.\<out>.exe        # 全 PASS 即 🔵 邏輯完整
```
模擬 exe 暫存放 `temp/`。

## ACPI / INF
```powershell
# ASL → AML（WDK ACPIVerify 內）
asl.exe windows_sources\pcie-rp1\acpi\rp1.asl       # 產 rp1.aml

# INF 驗證（x64 版 infverif；ERROR(1000) line 0 為此環境共通假象，看 WARNING/ERROR 內容）
infverif.exe /v windows_driver\<類別>\<驅動>.inf
```
INF 慣例：`Class=System`（多數）/對應類別 GUID；`[X.NT.Wdf] KmdfService=X` + `KmdfLibraryVersion=1.33`；
PnP id `ACPI\<_HID>` 對齊 `rp1.asl` 的 `_HID`。

## 細節參考
首次 KMDF 手動 build 與各框架配方緣由：`../Note/20260621-0331-rp1-i2s-audio-port.md`、
`../Note/20260621-0600-x64-logic-complete-loop-summary.md`、`../Note/20260621-0700-bespoke-kmdf-batch-summary.md`。
