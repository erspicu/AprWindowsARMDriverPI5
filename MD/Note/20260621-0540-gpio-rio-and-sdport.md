# GpioClx 真實 RIO 讀寫 + SdPort SD/MMC know-how

> 日期：2026-06-21 05:40

## 1. RP1 GPIO（GpioClx）真實腳位 I/O
- `rp1_gpio_hw.c`（移植 pinctrl-rp1.c）：
  - **RIO 區**：`OUT(0x00)/OE(0x04)/IN(0x08)`；atomic alias `SET(+0x2000)/CLR(+0x3000)` 做無鎖位元操作。
  - **pin→bank 映射**：RP1 三 bank（0-27/28-33/34-53），各 bank 子區偏移 = bank×0x4000。
  - **方向**：`RIO_OE + SET`(output)/`CLR`(input)；**FUNCSEL**：per-pin CTRL（gpio io 區，stride 8）寫 `FUNCSEL=5(GPIO)`。
- GpioClx I/O callback：`ReadGpioPins`/`WriteGpioPins` 用 `Buffer`（UINT64 位元，bit i = `PinNumberTable[i]`）；`ConnectIoPins` 看 `ConnectMode==ConnectModeOutput`。
- `PrepareController` 映 3 個 MMIO 區（gpio/rio/pads）；單一區時 degrade。

## 2. RP1/BCM2712 SD/MMC（SdPort miniport）
- 產出 `windows_driver/storage/rp1sd.sys`（ARM64, import sdport.sys）。
- **SdPort miniport（非 KMDF / 非 WDM 自寫）**：`DriverEntry` 只呼叫 `SdPortInitialize(DriverObject, RegistryPath, &SDPORT_INITIALIZATION_DATA)`；link **`sdport.lib`**，entry `GsDriverEntry`，**不需 wdf libs**。
- `SDPORT_INITIALIZATION_DATA`：16 個 callback（GetSlotCount/GetSlotCapabilities/Interrupt/IssueRequest/GetResponse/RequestDpc/Toggle/ClearEvents/Save/RestoreContext/Initialize/IssueBusOperation/GetCardDetectState/GetWriteProtectState/Cleanup）+ `PrivateExtensionSize`（per-slot 私有結構）。
- callback 簽章用 sdport.h 的函式 typedef（`SDPORT_GET_SLOT_COUNT` 等）前向宣告，定義端配對。
- `GetSlotCapabilities` 填 `SDPORT_CAPABILITIES`（SpecVersion、MaximumBlockSize=512、BaseClockFrequencyKhz、`Supported` bitfield: HighSpeed/Voltage33V…）。
- `Initialize(PrivateExtension, PhysicalBase, VirtualBase, Length, CrashdumpMode)` 把 VirtualBase 存進私有結構 = SDHCI 暫存器基底。
- **餘下需實機**：SDHCI 命令引擎（IssueRequest 程式化 Argument/TransferMode/Command、Interrupt 讀 Normal/Error Int Status、GetResponse 讀 Response、IssueBusOperation 設 clock/bus-width/reset）。

## 連帶可複用
- **BCM2712 SD（sdhci-brcmstb）**：可套同一 SdPort miniport 結構，換 SDHCI 暫存器細節。
