# Phase 2 — PortCls/WaveRT 音訊驅動 know-how（持續更新）

> 日期：2026-06-21 03:41（loop job 28957e8c 進行中）
> 主題：把 RP1 I2S 從 Phase 1（WDM 骨架 + HAL）推進成真正的 Windows 音訊驅動（PortCls + WaveRT）。
> 狀態：**基礎已編譯通過（ARM64）**；miniport 實作與 link 進行中。

## 專案
- `windows_sources/audio/rp1-i2s-portcls/`（重用 `../rp1-i2s/rp1_i2s_hw.c` 的 HAL）

## 架構（PortCls/WaveRT render endpoint）
- **adapter.cpp**：`DriverEntry` → `PcInitializeAdapterDriver`；`AddDevice` → `PcAddAdapterDevice`；`StartDevice` 內 `PcNewPort(CLSID_PortWaveRT)` + `PcNewPort(CLSID_PortTopology)`，各自 `Init` + `PcRegisterSubdevice`，再 `PcRegisterPhysicalConnection` 串接 wave bridge pin ↔ topo bridge pin。
- **minwave.{h,cpp}**：`CMiniportWaveRT : IMiniportWaveRT, CUnknown`。
- **minwavestream.{h,cpp}**：`CMiniportWaveRTStream : IMiniportWaveRTStream, CUnknown`；`SetState` 呼叫 HAL `Rp1I2sHwStart/Stop`，`AllocateAudioBuffer` 配置 cyclic buffer。
- **mintopo.{h,cpp}**：`CMiniportTopology : IMiniportTopology, CUnknown`。
- **tables.cpp**：`PCFILTER_DESCRIPTOR`（wave + topo）。

## 已驗證的關鍵 know-how（踩雷紀錄）

1. **用 WDK 的 `IMP_*` 巨集宣告 miniport 方法**，簽章必與標頭一致：
   `IMP_IMiniport`（GetDescription/DataRangeIntersection）、`IMP_IMiniportWaveRT`（Init/NewStream）、`IMP_IMiniportWaveRTStream`（SetFormat/SetState/GetPosition/AllocateAudioBuffer/FreeAudioBuffer/GetHWLatency/GetPositionRegister/GetClockRegister）、`IMP_IMiniportTopology`（Init）。位置：`Include\10.0.26100.0\km\portcls.h`。
2. **GUID 配置**：在 `common.h` 最前 `#define INITGUID` + `#include <initguid.h>` 再含 `portcls.h`/`ksmedia.h` → KS/PortCls GUID 以 `__declspec(selectany)` 在各 TU 定義並由 linker 合併（CLSID_PortWaveRT、KSDATAFORMAT_TYPE_AUDIO 等都靠這個）。
3. **`KSPIN_DESCRIPTOR` 最後是一個 union**（`Reserved` / `ConstrainedDataRanges…`）→ aggregate init **只能給一個值或直接省略**；給 `0, NULL` 會 `C2078 初始設定式太多`。本專案直接省略尾端 union 初始化。
4. **`PCFILTER_DESCRIPTOR` 含 `PinSize` 與 `NodeSize` 欄位**（在 PinCount/NodeCount 之前），需填 `sizeof(PCPIN_DESCRIPTOR)` / `sizeof(PCNODE_DESCRIPTOR)`。
5. **node pin 編號**：input = 1、output = 0（KSNODEPIN_STANDARD_IN/OUT）。
6. **C++ kernel 編譯旗標**：`/kernel /GS /Zc:wchar_t`，include 順序 `km\crt`→`km`→`shared`（**勿加 VC user-mode include**，理由同 Phase 1）。link 另加 `portcls.lib stdunk.lib ksguid.lib`（其餘同 Phase 1：`BufferOverflowFastFailK.lib ntoskrnl.lib hal.lib`，entry `GsDriverEntry`）。
7. **`stdunk.h` 警告 operator new/delete 即將移除** → 之後在驅動內自定 kernel `operator new/delete`（用 `ExAllocatePool2`/`ExFreePool`）。
8. **增量驗證**：`build.ps1 -CompileOnly` 只編譯不連結，邊寫 miniport 邊 compile-check，最後再 link。

## 待辦（後續 loop fire 繼續）
- [ ] `operator new/delete`（kernel）
- [ ] minwave / minwavestream / mintopo 實作（用 `IMP_*` 巨集）
- [ ] adapter.cpp（DriverEntry/AddDevice/StartDevice）
- [ ] rp1i2saud.inf（Media class + KSCATEGORY_AUDIO/RENDER 介面註冊）
- [ ] 完整 link → `rp1i2saud.sys`，交付 `windows_driver/audio/`
- [ ] 把 HAL 接進 stream 的 SetState/SetFormat
- [ ] 簽章載入（測試簽章）；實機需先打通 RP1 PCIe + ACPI 提供 I2S 資源

## 參考
- Phase 1 紀錄：`MD/Note/20260621-0331-rp1-i2s-audio-port.md`
- 硬體清單：`MD/Note/RPi5-Driver-Porting-Inventory.md`
- 規格諮詢：`tools/knowledgebase/gemini_query.py`（本次用它取得 PortCls 骨架與 build 配方）
