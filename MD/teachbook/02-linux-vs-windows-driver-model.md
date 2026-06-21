# 第 02 章：Linux vs Windows 驅動模型的根本差異

> 目標：理解為什麼移植驅動是「**重寫**」而非「重新編譯」——以及哪些東西可以沿用、哪些一定要重寫。

## 2.1 一張表看懂「能沿用 vs 必須重寫」

| 面向 | Linux | Windows | 移植時 |
|------|-------|---------|--------|
| 暫存器定義 / bit field | `#define` | `#define` | ✅ **直接沿用** |
| 初始化序列 / 演算法 | C | C | ✅ **沿用邏輯** |
| 通訊協定 | C | C | ✅ **沿用** |
| 暫存器讀寫 | `readl/writel` | `READ/WRITE_REGISTER_ULONG` | ✏️ 改 API（語意相近） |
| 裝置列舉 / 綁定 | Device Tree + `platform_driver` | ACPI/PnP + INF | 🔁 **重寫** |
| 中斷 | `request_irq` | KMDF ISR/DPC、GpioClx callback | 🔁 **重寫** |
| 記憶體 / DMA | `kmalloc`/`dma_alloc_*` | `WdfCommonBuffer`/`MmAllocate*` | 🔁 **重寫** |
| 對上層的介面 | subsystem（netdev/i2c_adapter…） | class extension（NDIS/SpbCx…） | 🔁 **重寫** |
| 鎖 / 同步 | spinlock/mutex | `WDFSPINLOCK`/`KSPIN_LOCK` | 🔁 **重寫** |

**核心觀念**：驅動可拆成兩層——
- **HAL（硬體抽象層）**：操作暫存器的純邏輯（狀態機）。**這層幾乎可 1:1 沿用 Linux。**
- **OS glue（介接層）**：列舉、中斷、DMA、電源、對上層介面。**這層在 Windows 完全重寫。**

本專案的整個方法論就是把這兩層**乾淨切開**（見 [第 06 章](06-porting-methodology.md)）。

## 2.2 同一件事的兩種寫法：讀一個暫存器

**Linux：**
```c
#include <linux/io.h>
u32 v = readl(base + DW_IC_STATUS);   /* base 來自 ioremap() */
writel(0, base + DW_IC_ENABLE);
```

**Windows（kernel）：**
```c
#include <wdm.h>
ULONG v = READ_REGISTER_ULONG((volatile ULONG *)(Base + DW_IC_STATUS));
WRITE_REGISTER_ULONG((volatile ULONG *)(Base + DW_IC_ENABLE), 0);
```

語意幾乎一樣，差別只在 API 名稱與型別。**暫存器 offset（`DW_IC_STATUS` 等）完全不用改**——那是硬體事實。

> ⚠️ ARM64 要小心**記憶體序與 barrier**：`READ/WRITE_REGISTER_*` 在 Windows 是有序的 volatile 存取，
> 對應 Linux 的 `readl/writel`（含 barrier）。詳見 [第 09 章](09-arm64-and-signing.md)。

## 2.3 裝置怎麼「被認得」？DT vs ACPI+INF

**Linux**：核心讀 **Device Tree**，看到 `compatible = "snps,designware-i2c"`，就去比對註冊過的
`platform_driver`，呼叫它的 `.probe()`。

```c
static const struct of_device_id dw_i2c_of_match[] = {
    { .compatible = "snps,designware-i2c" }, { }
};
static struct platform_driver dw_i2c_driver = {
    .probe = dw_i2c_plat_probe,
    .driver = { .name = "i2c_designware", .of_match_table = dw_i2c_of_match },
};
```

**Windows**：靠 **ACPI 描述裝置**（`_HID`）+ **INF 把驅動綁到那個 `_HID`** + 框架呼叫你的 `EvtDeviceAdd`。

```asl
// ACPI (rp1.asl) 描述一個 I2C 裝置
Device (I2C0) { Name (_HID, "RPI50001") Name (_UID, 0)
    Method (_CRS) { ... GpioInt(...) { 7 } ... } }
```
```ini
; INF 把 rp1i2c.sys 綁到 ACPI\RPI50001
[Standard.NTARM64]
%rp1i2c.DeviceDesc% = rp1i2c_Device, ACPI\RPI50001
```
```c
// 驅動入口
NTSTATUS DriverEntry(PDRIVER_OBJECT drv, PUNICODE_STRING reg) {
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, EvtDeviceAdd);   // 框架在裝置出現時呼叫
    return WdfDriverCreate(drv, reg, ..., &config, ...);
}
```

→ Device Tree 那套「compatible 比對」在 Windows 變成「**ACPI `_HID` + INF 綁定**」三件套。
（DT→ACPI 的細節見 [第 04 章](04-device-tree-to-acpi.md)。）

## 2.4 「對上層的介面」才是重寫的大頭

Linux 的驅動會把自己註冊進某個 **subsystem**：I2C 驅動註冊成 `i2c_adapter`、網卡註冊成 `net_device`…
上層（i2c-core、網路堆疊）透過 subsystem 的 callback 操作你。

Windows 對應的是 **class extension（Cx）框架**：

| 裝置類 | Linux subsystem | Windows 框架（你要實作的 callback） |
|--------|-----------------|--------------------------------------|
| I2C/SPI | `i2c_adapter`/`spi_master` | **SpbCx**（`EvtSpbControllerLock`/`EvtSpbIoRead`…） |
| GPIO | `gpio_chip` | **GpioClx**（~16 個 callback） |
| 網卡 | `net_device` | **NDIS miniport**（`MiniportSend`/`MiniportOidRequest`…） |
| SD host | `mmc_host` | **SdPort miniport** |
| 音訊 | ALSA SoC (`snd_soc`) | **PortCls / WaveRT** |
| 顯示 | DRM/KMS | **WDDM** |

這層是「**把你的 HAL 接到 Windows 的世界**」，**沒有對應關係可抄**，必須照各 Cx 框架的合約重寫。
好消息是：你的 HAL（暫存器狀態機）就是這些 callback 內部要呼叫的東西——只要 HAL 乾淨，glue 就只是「翻譯」。

## 2.5 為什麼不能直接編譯 Linux 驅動？

- **API 完全不同**：`request_irq`、`dma_alloc_coherent`、`i2c_add_adapter`… 在 Windows 不存在。
- **執行模型不同**：Linux 的 `probe`/中斷/work queue ↔ Windows 的 PnP 狀態機/ISR/DPC/work item。
- **GPL 授權**：直接連結 Linux 核心碼有授權問題；移植是「**參考暫存器邏輯重寫**」，乾淨許多。

→ 所以正解是：**讀 Linux 驅動，萃取「暫存器邏輯」，用 Windows 框架重新實作介接層**。

## 本章重點

- 移植 = **重寫**，但可把驅動拆成 **HAL（可沿用）** + **OS glue（重寫）**。
- 暫存器 offset/序列/演算法**直接沿用**；列舉/中斷/DMA/上層介面**重寫**。
- DT 的「compatible 比對」→ Windows 的「**ACPI `_HID` + INF 綁定 + 框架 callback**」。
- 重寫的大頭是「**對上層的 class extension 介面**」（SpbCx/GpioClx/NDIS…）。

➡️ 下一章：[Windows 驅動框架地圖](03-windows-driver-frameworks.md)
