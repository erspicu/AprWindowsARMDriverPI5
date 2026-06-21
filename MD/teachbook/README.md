# Teachbook — Windows 驅動移植教學 × Raspberry Pi 5 硬體規格書

> 以「把 **Raspberry Pi 5**（BCM2712 + RP1）的 Linux 驅動移植到 **Windows on ARM (ARM64)**」這個真實專案為教材，
> 帶你從**基本觀念**走到**逐一硬體的規格與移植**。每章盡量附**真實 code sample**（取自本專案 `windows_sources/`），
> 兼具「**教學書**」與「**Pi5 硬體移植規格書**」兩種功能。
>
> 適合：想學 Windows 驅動開發、想了解 Linux→Windows 驅動移植、或想認識 Pi5 硬體的人。
> 預備知識：會看 C、對作業系統/硬體有基本概念即可；不需先懂驅動。

本書持續撰寫中（✍️ 撰寫中 / ✅ 已完成 / 🔲 規劃中）。

---

## 第一部：基礎觀念（先讀）

| 章 | 標題 | 狀態 |
|----|------|------|
| 01 | [Pi5 是什麼？雙晶片架構總覽（BCM2712 + RP1）](01-pi5-architecture.md) | ✅ |
| 02 | [Linux vs Windows 驅動模型的根本差異](02-linux-vs-windows-driver-model.md) | ✅ |
| 03 | [Windows 驅動框架地圖（KMDF/SpbCx/GpioClx/NDIS/PortCls/WDDM…）](03-windows-driver-frameworks.md) | ✅ |
| 04 | [硬體描述：從 Device Tree 到 ACPI](04-device-tree-to-acpi.md) | ✅ |
| 05 | [命脈：PCIe 與 RP1 列舉](05-pcie-and-rp1-enumeration.md) | ✅ |
| 06 | [移植方法論：HAL 抽離 + OS glue + regio shim](06-porting-methodology.md) | ✅ |
| 07 | [x64 模擬驗證：不用硬體就能驗狀態機](07-x64-simulation-pattern.md) | ✅ |
| 08 | [用實機校正：Pi5 SSH 與「規格缺漏 A/B 兩類」](08-hardware-truth-and-ab-gaps.md) | ✅ |
| 09 | [ARM64 記憶體模型、MMIO barrier、驅動簽章](09-arm64-and-signing.md) | ✅ |
| 10 | [建置工具鏈與你的第一個驅動（手把手 toolchain）](10-build-your-first-driver.md) | ✅ |

## 第二部：硬體規格與移植

每章結構固定：**裝置概述 → 規格/暫存器重點 → Linux 源碼參考 → Windows 框架對應 → 移植要點 →
code sample → Pi5 實測事實 → 移植狀態**。

### A. BCM2712 SoC 核心

| 章 | 裝置 | 框架 | 狀態 |
|----|------|------|------|
| A1 | PCIe Root Complex | inbox pci.sys + ACPI | 🔲 |
| A2 | [VideoCore 韌體介面 / Mailbox](A02-mailbox.md) | KMDF | ✅ |
| A3 | BCM2712 clocks / power | 韌體/ACPI | 🔲 |
| A4 | GIC-400 / L2 中斷控制器 | ACPI MADT | 🔲 |
| A5 | [SDHCI（SD/eMMC）](A05-sdhci-sdport.md) | SdPort | ✅ |
| A6 | [BCM2712 GPIO / pinctrl](A06-bcm2712-gpio.md) | GpioClx | ✅ |
| A7 | [BCM2712 DMA（含 DMA40）](A07-bcm2712-dma40.md) | KMDF | ✅ |
| A8 | [BCM2712 I2C（BSC）](A08-bcm2712-i2c.md) | SpbCx | ✅ |
| A9 | [BCM2712 SPI](A09-bcm2712-spi.md) | SpbCx | ✅ |
| A10 | PL011 UART（除錯） | SerCx2 / inbox | 🔲 |
| A11 | [RNG（iproc-rng200）](A11-rng.md) | KMDF | ✅ |
| A12 | [Watchdog / PM](A12-watchdog.md) | KMDF | ✅ |
| A13 | 溫度 / 系統計時器 | ACPI | 🔲 |
| A14 | IOMMU | DMA remapping | 🔲 |

### B. RP1 南橋

| 章 | 裝置 | 框架 | 狀態 |
|----|------|------|------|
| B1 | RP1 韌體 / mailbox | KMDF | 🔲 |
| B2 | [RP1 clocks（PLL）](B02-rp1-clocks.md) | KMDF | ✅ |
| B3 | [RP1 GPIO / pinctrl](B03-rp1-gpio.md) | GpioClx | ✅ |
| B4 | [RP1 Gigabit Ethernet（Cadence GEM）](B04-rp1-ethernet-gem.md) | NDIS | ✅ |
| B5 | RP1 USB3 ×2（DesignWare DWC3） | inbox xHCI | 🔲 |
| B6 | RP1 DMA（DesignWare AXI-DMA） | KMDF | 🔲 |
| B7 | RP1 UART ×6（PL011-AXI） | SerCx2 | 🔲 |
| B8 | [RP1 SPI ×8（DesignWare SSI）](B08-rp1-designware-spi.md) | SpbCx | ✅ |
| B9 | [RP1 I2C ×7（DesignWare）](B09-rp1-designware-i2c.md) | SpbCx | ✅（範本章）|
| B10 | [RP1 PWM ×2](B10-rp1-pwm.md) | KMDF | ✅ |
| B11 | RP1 SD/MMC（DesignWare MSHC） | SdPort | 🔲 |
| B12 | [RP1 ADC / 溫度](B12-rp1-adc.md) | KMDF | ✅ |
| B13 | [RP1 PIO（獨有可程式 I/O）](B13-rp1-pio.md) | KMDF 自訂 | ✅ |
| B14 | [RP1 I2S ×3（DesignWare）](B14-rp1-i2s.md) | PortCls | ✅ |
| B15 | RP1 類比音訊輸出 | PortCls | 🔲 |

### C. 多媒體（顯示 / GPU / 相機 / 編解碼）

| 章 | 裝置 | 框架 | 狀態 |
|----|------|------|------|
| C1 | HDMI ×2 | WDDM | 🔲 |
| C2 | HVS / PixelValve（顯示合成） | WDDM | 🔲 |
| C3 | V3D GPU | WDDM 完整 GPU | 🔲 |
| C4 | HEVC 解碼器 | Media Foundation / DXVA | 🔲 |
| C5 | ISP（PiSP Back End） | AVStream / MFT | 🔲 |
| C6 | MIPI CSI 相機前端 ×2 | AVStream | 🔲 |
| C7 | MIPI DSI / DPI / VEC 顯示輸出 | WDDM | 🔲 |

### D. 外接 / 板載 / 韌體

| 章 | 裝置 | 框架 | 狀態 |
|----|------|------|------|
| D1 | WiFi（CYW43455，SDIO） | NDIS 802.11 | 🔲 |
| D2 | [Bluetooth（BCM43438，UART HCI）](D02-bluetooth.md) | bthport | ✅ |
| D3 | Ethernet PHY（BCM54213） | 併入 NDIS | 🔲 |
| D4 | 散熱風扇（PWM-fan）/ 熱區 | KMDF + 熱區 | 🔲 |
| D5 | 電源鍵 / 狀態 LED | HID / KMDF | 🔲 |
| D6 | RTC（韌體型） | KMDF / ACPI | 🔲 |
| D7 | PMIC / regulators | 韌體 / ACPI | 🔲 |

---

## 如何使用本書

- **想入門**：從第一部 01→10 順讀，建立觀念後再挑有興趣的硬體章。
- **想移植某個裝置**：直接翻第二部對應章，每章自成一篇規格 + 移植指南。
- **想看真實程式**：每章 code sample 都指到本專案 `windows_sources/<類別>/<專案>/`，可直接對照完整原始碼。

> 相關文件：移植狀態 → [`../Note/RPi5-Porting-Status.md`](../Note/RPi5-Porting-Status.md)；
> 硬體全清單 → [`../Note/RPi5-Driver-Porting-Inventory.md`](../Note/RPi5-Driver-Porting-Inventory.md)；
> 操作 how-to → [`../Skill/`](../Skill/)。
