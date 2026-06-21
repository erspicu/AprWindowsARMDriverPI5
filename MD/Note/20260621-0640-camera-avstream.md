# 相機 RP1 CFE — AVStream minidriver 骨架 know-how

> 日期：2026-06-21 06:40　｜ 產出 `windows_driver/camera/rp1cfe.sys`（ARM64, import ks.sys）

## 完成
- AVStream（KS）minidriver 註冊骨架：`DriverEntry` → `KsInitializeDriver(DriverObject, RegistryPath, &KSDEVICE_DESCRIPTOR)`。
- `KSDEVICE_DESCRIPTOR`：`Dispatch`(KSDEVICE_DISPATCH*)、`FilterDescriptorsCount`、`FilterDescriptors`、`Version`(=`KSDEVICE_DESCRIPTOR_VERSION` 0x100)。
- `KSDEVICE_DISPATCH`：14 callback（Add/Start/PostStart/QueryStop/CancelStop/Stop/QueryRemove/CancelRemove/Remove/QueryCapabilities/SurpriseRemoval/QueryPower/SetPower/QueryInterface）；僅 `Add`(=`PFNKSDEVICECREATE`, `NTSTATUS(PKSDEVICE)`) 提供，其餘 NULL 走 KS 預設。

## 手動 build 踩雷（重要）
1. **必須 C++（.cpp）**：`ks.h` 在 C 路徑用 `DEFINE_GUIDEX`+`CDECL` → 編譯錯（`CDECL 之後必須有 (`）。C++ 走 `__declspec(uuid)` 路徑可過。`DriverEntry` 加 `extern "C"`。
2. **`ks.h` 用到 Win32 型別/巨集**（kernel 標頭沒定義）：include `<ks.h>` 前需先
   ```c
   typedef int BOOL;            // KsRegisterFilterWithNoKSPins 等用 BOOL*
   #ifndef WINAPI
   #define WINAPI __stdcall     // KsAcquireCachedMdl/KsReleaseCachedMdl 用 WINAPI
   #endif
   ```
3. link **`ks.lib` + `ksguid.lib`**（+ 標準 libcntpr/BufferOverflowFastFailK/ntoskrnl/hal），entry `GsDriverEntry`。ks.h 在 `shared\`（已在 include 集）。

## 餘下（capture filter，大工程，需實機）
- `KsCreateFilterFactory` + `KSFILTER_DESCRIPTOR`：pin（KSPIN_DESCRIPTOR_EX）= MIPI-CSI2 sensor 輸入 → PiSP ISP → video 輸出 pin。
- KSPIN data range（`KS_DATAFORMAT_VIDEOINFOHEADER` / NV12 等）、buffer/DMA 佇列（KSPIN process）、sensor I2C 控制（透過 #5 rp1i2c）。
- 與 MF（Media Foundation）相機堆疊對接。
