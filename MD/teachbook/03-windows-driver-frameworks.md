# 第 03 章：Windows 驅動框架地圖

> 目標：認識 Windows 的各種驅動框架，知道「**某類裝置該用哪個框架**」，以及它們彼此的關係。
> 移植時挑錯框架，等於整個介接層白寫。

## 3.1 最底層：WDM，與它的上層 KMDF

- **WDM（Windows Driver Model）**：最原始的核心驅動模型，你要自己處理 IRP、PnP 狀態機、電源…冗長易錯。
- **KMDF（Kernel-Mode Driver Framework）**：包在 WDM 之上的**現代框架**，用「物件 + 事件 callback」取代手刻 IRP。
  **沒有現成 class 框架時，預設就用 KMDF。**

```c
// KMDF 的基本骨架（每個 KMDF 驅動都長這樣）
NTSTATUS DriverEntry(PDRIVER_OBJECT drv, PUNICODE_STRING reg) {
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, EvtDeviceAdd);
    return WdfDriverCreate(drv, reg, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);
}
NTSTATUS EvtDeviceAdd(WDFDRIVER drv, PWDFDEVICE_INIT init) {
    // 設 PnP/Power callback、建 device、map MMIO…
}
NTSTATUS EvtPrepareHardware(WDFDEVICE dev, WDFCMRESLIST raw, WDFCMRESLIST trans) {
    // 從資源清單拿 CmResourceTypeMemory → MmMapIoSpaceEx() 取得 MMIO 虛擬位址
}
```

## 3.2 「Class Extension（Cx）」框架：替特定裝置類別代勞

對常見裝置類，微軟提供 **class extension**：它幫你處理該類的共通協定，你只要實作**硬體相關的 callback**。
這就是「**把 HAL 接上 Windows**」的接點。

| 框架 | 裝置類 | 你要實作的重點 callback | 本專案用例 |
|------|--------|--------------------------|-----------|
| **SpbCx** | I2C / SPI 控制器 | `EvtSpbTargetConnect`、`EvtSpbControllerLock`、`EvtSpbIoRead/Write` | `rp1i2c`/`rp1spi`/`bcm2712i2c`/`bcm2712spi` |
| **GpioClx** | GPIO 控制器 | ~16 個：`ConnectIoPins`、`ReadGpioPins`、`WriteGpioPins`、`EnableInterrupt`… | `rp1gpio`/`bcm2712gpio` |
| **SerCx2** | 序列埠 (UART) | `EvtSerCx2…`（多走 inbox `SerPl011`） | RP1/BCM UART |
| **SdPort** | SD/eMMC host | miniport：`GetSlotCapabilities`、`IssueRequest`、`IssueBusOperation`、`Interrupt` | `rp1sd`（SDHCI） |
| **NDIS** | 網路卡 | miniport：`MiniportSend`、`MiniportReturnNetBufferLists`、`MiniportOidRequest` | `rp1gem`（Ethernet） |
| **PortCls** | 音訊 | WaveRT miniport：DataRange、stream、DMA | `rp1i2saud` |
| **WDDM** | 顯示 / GPU | DOD（Display-Only）→ 完整 render（DxgkDdi…） | `rp1vc4dod`、`rp1v3d` |
| **AVStream** | 相機 / 影像擷取 | KS filter/pin、capture | `rp1cfe` |
| **bthport** | Bluetooth | 序列 HCI / BthMini | `btbcm` |

> 觀念：**Cx 框架 = 上層協定的「半成品」**。你補的是「跟硬體講話」那一半（呼叫你的 HAL），
> 協定那一半（i2c 傳輸排程、網路封包佇列…）框架幫你扛。

## 3.3 「不用寫驅動，但要寫 ACPI」的那些

有些東西作業系統**內建處理**，你只要在 ACPI 把它**描述對**：

| 東西 | 怎麼處理 |
|------|----------|
| GIC 中斷控制器 | ACPI **MADT** 表 |
| Arch timer / 系統計時器 | ACPI **GTDT** 表 |
| PCIe ECAM | ACPI **MCFG** + `PNP0A08` |
| 一般中斷/記憶體資源 | 裝置的 `_CRS`（`Interrupt`/`Memory`/`GpioInt`） |

→ 這類屬於「**純軟體交付物**」，沒有 `.sys`，但寫對 ASL 一樣是工作量。

## 3.4 怎麼選框架？決策樹

```
這個裝置是…
├─ I2C / SPI 控制器？           → SpbCx
├─ GPIO 控制器？               → GpioClx
├─ UART？                      → SerCx2（優先用 inbox SerPl011）
├─ SD / eMMC host？            → SdPort miniport
├─ 網路卡？                    → NDIS miniport
├─ 音訊？                      → PortCls / WaveRT
├─ 顯示 / GPU？                → WDDM（先 DOD 點亮，再 render）
├─ 相機 / 影像？               → AVStream
├─ Bluetooth？                 → bthport
├─ 中斷控制器 / timer / ECAM？ → 不寫驅動，寫 ACPI（MADT/GTDT/MCFG）
└─ 以上都不是（PWM/ADC/RNG/DMA/clock/mailbox…）→ bespoke KMDF
```

最後一類「**bespoke KMDF**」是本專案大量用的：沒有現成 Cx 的小周邊，就用裸 KMDF + 自訂 IOCTL/介面，
配上乾淨 HAL（見 [第 06 章](06-porting-methodology.md)）。

## 3.5 連結時要帶哪些 lib（速查）

| 框架 | entry | 關鍵 lib |
|------|-------|----------|
| KMDF | `FxDriverEntry` | `wdfdriverentry.lib` `wdfldr.lib` |
| GpioClx | — | `msgpioclxstub.lib` |
| SdPort | — | `sdport.lib` |
| NDIS | — | `ndis.lib` |
| PortCls | — | `portcls.lib` `stdunk.lib` `ksguid.lib` |
| AVStream | — | `ks.lib` `ksguid.lib`（**C++ 編譯**） |
| WDDM | — | `displib.lib` |

（完整建置配方見 [`../Skill/build-toolchain.md`](../Skill/build-toolchain.md)。）

## 本章重點

- 沒現成框架 → **KMDF**；常見裝置類 → 對應的 **Cx 框架**（SpbCx/GpioClx/NDIS/SdPort/PortCls/WDDM/AVStream）。
- Cx 框架幫你扛上層協定，你只補「跟硬體講話」那半——也就是呼叫你的 HAL。
- 中斷控制器/timer/ECAM **不寫驅動**，寫 ACPI（MADT/GTDT/MCFG）。
- 小周邊（PWM/ADC/RNG/DMA/clock…）用 **bespoke KMDF**。

➡️ 下一章：[硬體描述：從 Device Tree 到 ACPI](04-device-tree-to-acpi.md)
