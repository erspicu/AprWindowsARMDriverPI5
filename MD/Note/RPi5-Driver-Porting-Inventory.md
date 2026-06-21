# Raspberry Pi 5 硬體裝置清單與 Windows on ARM 驅動移植對照表

> 目標：將 Raspberry Pi 5 的 Linux 驅動移植到 **Windows on ARM (ARM64)**。
> 資料來源：`sources/`（raspberrypi/linux kernel），以 device tree 為硬體權威清單，並交叉比對 `drivers/` 內實際原始碼路徑。
> 建立日期：2026-06-21

---

## 1. 系統架構概述

Raspberry Pi 5 由**兩顆主要晶片**組成，移植時要分開看待：

| 晶片 | 角色 | 連接方式 |
|------|------|----------|
| **BCM2712** | 主 SoC（4× Cortex-A76、GPU、顯示、記憶體控制器） | 直接在 SoC bus 上 |
| **RP1** | I/O 南橋（USB / Ethernet / GPIO / I2C / SPI / UART / PWM / 類比影音 / 相機） | **透過 PCIe Gen2 ×4 掛在 BCM2712 底下** |

```
            +-------------------- BCM2712 SoC --------------------+
            |  Cortex-A76 ×4   GIC-400   VideoCore VII GPU(V3D)   |
            |  HDMI ×2  HVS/MOP  HEVC dec  ISP(PiSP)  IOMMU  DMA  |
            |  SD(SDHCI)  SPI  I2C  PL011  RNG  Thermal  Mailbox  |
            |  VideoCore 韌體 (管理 clock / power / PMIC)          |
            +------------------------+----------------------------+
                                     | PCIe Gen2 ×4 (pcie2)
                                     v
            +-------------------- RP1 南橋 -----------------------+
            |  USB3 ×2(dwc3)  Gigabit Ethernet(MACB)  DMA        |
            |  GPIO/pinctrl  UART ×6  SPI ×8  I2C ×7  PWM ×2     |
            |  I2S ×3  類比音訊  ADC  SD/MMC ×2  PIO             |
            |  MIPI CSI ×2(相機)  DSI ×2  DPI  VEC(類比電視)     |
            |  RP1 韌體 / mailbox  clocks                        |
            +----------------------------------------------------+
```

### 移植本質（務必先理解）
- **這是「重寫」，不是「重新編譯」。** Linux 與 Windows 驅動模型完全不同。
- **可沿用**：暫存器定義、bit field、初始化序列、通訊協定、演算法（核心價值）。
- **必須重寫**：OS 介接層（記憶體、中斷、DMA、電源、bus 列舉、使用者介面）。
- **硬體描述轉換**：Linux 用 **Device Tree (DT)**；Windows 用 **ACPI (ASL)**。`bcm2712-rpi-5-b.dts` 的拓樸要翻成 ACPI 表。
- **PCIe 是關鍵路徑**：RP1 上的所有 I/O 都掛在 PCIe 底下，因此 **BCM2712 PCIe Root Complex 驅動 + RP1 列舉**是整個移植的命脈，沒有它 RP1 全部裝置都看不到。

### Windows 驅動框架對照（後續表格會用到的縮寫）
| 縮寫 | 用途 |
|------|------|
| KMDF / UMDF | 核心 / 使用者模式驅動框架（通用裝置） |
| NDIS | 網路介面卡 |
| SerCx2 | 序列埠 (UART) |
| SpbCx | Simple Peripheral Bus（I2C / SPI） |
| GpioClx | GPIO 控制器 |
| WDDM | 顯示 / GPU |
| AVStream | 相機 / 影像擷取 |
| PortCls | 音訊 |
| ACPI | 由韌體/作業系統處理，通常**不需寫驅動**（但要寫 ASL 描述） |

---

## 2. 移植優先順序（建議分階段）

| 階段 | 內容 | 為何 |
|------|------|------|
| **Tier 0 — 開機命脈** | PCIe RC、BCM2712 clocks/firmware、GIC(ACPI)、SD 卡(SDHCI)、GPIO/pinctrl、RP1 firmware/mailbox/clocks | 沒有這些無法開機或看不到 RP1 |
| **Tier 1 — 核心 I/O** | RP1 Ethernet、RP1 USB3 ×2、RP1 UART(除錯)、DMA | 基本可用系統 |
| **Tier 2 — 周邊匯流排** | I2C、SPI、PWM、ADC、RTC、RP1 MMC | 一般周邊 |
| **Tier 3 — 多媒體** | HDMI/顯示(vc4)、V3D GPU、相機(CSI/PiSP)、HEVC、DSI/DPI/VEC、I2S 音訊 | 工程量最大 |
| **Tier 4 — 無線** | WiFi(SDIO)、Bluetooth(UART) | 韌體與 stack 複雜 |

---

## 3. BCM2712 SoC 平台裝置（需 ACPI 描述）

> 這些是 SoC 內建的記憶體映射裝置（MMIO platform device）。在 Windows 上需以 ACPI `_HID/_CRS` 描述，並寫 KMDF/各類 class driver。

| 裝置 | DT compatible | Linux 原始碼（已驗證） | Windows 框架 | 難度 | 備註 |
|------|---------------|------------------------|--------------|:----:|------|
| **PCIe Root Complex** | `brcm,bcm2712-pcie` | `drivers/pci/controller/pcie-brcmstb.c` | KMDF bus / PCI 驅動堆疊 | ★★★★ | **Tier 0 命脈**，RP1 靠它列舉 |
| VideoCore 韌體介面 | `raspberrypi,bcm2835-firmware` | `drivers/firmware/raspberrypi.c` | KMDF（mailbox 為基礎） | ★★★★ | 管 clock/power/溫度/PMIC，**核心相依** |
| 韌體 clocks | `raspberrypi,firmware-clocks` | `drivers/clk/bcm/clk-raspberrypi.c` | 併入韌體驅動 | ★★★ | |
| 韌體 power domains | `raspberrypi,bcm2835-power` | `drivers/pmdomain/bcm/raspberrypi-power.c` | 併入韌體驅動 / ACPI 電源 | ★★★ | |
| Mailbox (VC) | `brcm,bcm2835-mbox` | `drivers/mailbox/bcm2835-mailbox.c` | KMDF | ★★★ | 韌體溝通管道 |
| GIC-400 中斷控制器 | `arm,gic-400` | `drivers/irqchip/irq-gic.c` | **ACPI MADT（OS 內建）** | — | 不需寫驅動，寫 ACPI 即可 |
| L2 中斷控制器 | `brcm,l2-intc` / `brcm,bcm7271-l2-intc` | `drivers/irqchip/irq-brcmstb-l2.c` | ACPI 串接中斷 | ★★★ | 級聯中斷描述較麻煩 |
| SD 卡控制器 (SDHCI) | `brcm,bcm2712-sdhci` | `drivers/mmc/host/sdhci-brcmstb.c` | sdport miniport | ★★★ | **Tier 0** 開機媒體 |
| GPIO (主) | `brcm,brcmstb-gpio` | `drivers/gpio/gpio-brcmstb.c` | GpioClx | ★★★ | |
| Pinctrl | `brcm,bcm2712c0-pinctrl` | `drivers/pinctrl/bcm/pinctrl-brcmstb-bcm2712.c` | 併入 GpioClx | ★★★ | AON pinctrl 另有節點 |
| DMA 控制器 | `brcm,bcm2712-dma` | `drivers/dma/bcm2835-dma.c` | Windows DMA / KMDF | ★★★★ | |
| IOMMU | `brcm,bcm2712-iommu` | `drivers/iommu/bcm2712-iommu.c` | DMA remapping（高難度） | ★★★★ | |
| I2C (brcmstb) | `brcm,brcmstb-i2c` | `drivers/i2c/busses/i2c-brcmstb.c` | SpbCx | ★★ | |
| I2C (bcm2835) | `brcm,bcm2711-i2c` | `drivers/i2c/busses/i2c-bcm2835.c` | SpbCx | ★★ | |
| SPI (bootloader EEPROM) | `brcm,bcm2835-spi` | `drivers/spi/spi-bcm2835.c` | SpbCx | ★★ | spi10 |
| PL011 UART | `arm,pl011` | `drivers/tty/serial/amba-pl011.c` | SerCx2（PL011 有 inbox 支援） | ★★ | 可走 ACPI SPCR |
| AON UART (8250) | `brcm,bcm7271-uart` | `drivers/tty/serial/8250/8250_bcm7271.c` | SerCx2 | ★★ | |
| RNG | `brcm,bcm2711-rng200` | `drivers/char/hw_random/iproc-rng200.c` | KMDF（或不移植） | ★ | |
| 溫度感測 / AVS | `brcm,bcm2711-thermal` | `drivers/thermal/broadcom/bcm2711_thermal.c` | Windows 熱區 (ACPI) | ★★ | |
| PM / Watchdog | `brcm,bcm2712-pm` | `drivers/watchdog/` (brcmstb) | KMDF watchdog | ★★ | |
| Reset 控制器 | `brcm,brcmstb-reset` | `drivers/reset/reset-brcmstb.c` | 併入各驅動 | ★ | |
| 系統計時器 | `brcm,bcm2835-system-timer` | `drivers/clocksource/bcm2835_timer.c` | **ACPI GTDT（arch timer）** | — | 不需驅動 |
| USB OTG (內建) | `brcm,bcm2835-usb` | `drivers/usb/dwc2/` | Windows USB | ★★★ | Pi5 主要 USB 在 RP1，此為次要 |

### BCM2712 多媒體（顯示 / GPU / 編解碼，工程量最大）

| 裝置 | DT compatible | Linux 原始碼（已驗證） | Windows 框架 | 難度 | 備註 |
|------|---------------|------------------------|--------------|:----:|------|
| HDMI ×2 | `brcm,bcm2712-hdmi0/1` | `drivers/gpu/drm/vc4/vc4_hdmi.c` | WDDM display miniport | ★★★★★ | |
| HVS / 顯示合成 | `brcm,bcm2712-hvs` | `drivers/gpu/drm/vc4/vc4_hvs.c` | WDDM | ★★★★★ | |
| PixelValve / MOP / MOPLET | `brcm,bcm2712-pixelvalve0/1`、`-mop`、`-moplet` | `drivers/gpu/drm/vc4/` | WDDM | ★★★★★ | |
| **V3D GPU** | `brcm,2712-v3d` | `drivers/gpu/drm/v3d/v3d_drv.c` | **WDDM 完整 GPU 驅動** | ★★★★★+ | 工程量最大；Windows 無對應，需全新 |
| HEVC 解碼器 | `brcm,bcm2712-hevc-dec` | `drivers/media/platform/raspberrypi/hevc_dec/hevc_d.c` | Media Foundation / DXVA | ★★★★ | |
| ISP (PiSP Back End) | `raspberrypi,pispbe` | `drivers/media/platform/raspberrypi/pisp_be/pisp_be.c` | AVStream / MFT | ★★★★ | 影像處理流水線 |
| MIP (PCIe MSI) | `brcm,bcm2712-mip` | (irqchip / PCI MSI) | ACPI / PCI MSI | ★★★ | |

---

## 4. RP1 南橋裝置（透過 PCIe 連接）

> RP1 是一顆獨立 I/O 晶片，所有暫存器都在 PCIe BAR 內（基底 `0xc0_40000000`）。
> Windows 上需先讓 PCIe 列舉出 RP1，再以 **multi-function / bus driver** 把底下子裝置分出來。
> RP1 內大量採用**業界標準 IP（Synopsys DesignWare、Cadence、ARM PrimeCell）**，這些 IP 在 Windows 生態可能有參考實作或暫存器文件可循，是**重用機會較高**的部分。

| 裝置 | DT compatible | Linux 原始碼（已驗證） | Windows 框架 | 難度 | 備註 |
|------|---------------|------------------------|--------------|:----:|------|
| **RP1 韌體 / mailbox** | `raspberrypi,rp1-firmware` / `raspberrypi,rp1-mbox` | `drivers/firmware/rp1-fw.c`、`drivers/mailbox/rp1-mailbox.c` | KMDF | ★★★★ | **Tier 0**，gating RP1 子系統 |
| **RP1 clocks** | `raspberrypi,rp1-clocks` | `drivers/clk/clk-rp1.c` | KMDF clock | ★★★★ | **Tier 0** |
| RP1 SDIO clock | `raspberrypi,rp1-sdio-clk` | `drivers/clk/clk-rp1-sdio.c` | 併入 clock | ★ | |
| **RP1 GPIO / pinctrl** | `raspberrypi,rp1-gpio` | `drivers/pinctrl/pinctrl-rp1.c` | GpioClx | ★★★★ | **Tier 0**，腳位多工核心 |
| **RP1 Gigabit Ethernet** | `raspberrypi,rp1-gem` / `cdns,macb` | `drivers/net/ethernet/cadence/macb_main.c` | **NDIS 6.x miniport** | ★★★★ | Cadence GEM 標準 IP |
| **RP1 USB3 ×2** | `snps,dwc3` | `drivers/usb/dwc3/core.c` | Windows USB（xHCI 相容性待查） | ★★★★ | DesignWare DWC3 |
| **RP1 DMA** | `snps,axi-dma-1.01a` | `drivers/dma/dw-axi-dmac/dw-axi-dmac-platform.c` | KMDF DMA | ★★★★ | DesignWare AXI-DMA |
| RP1 UART ×6 | `arm,pl011-axi` | `drivers/tty/serial/amba-pl011.c` | SerCx2 | ★★ | PrimeCell PL011（AXI 變體） |
| RP1 SPI ×8 | `snps,dw-apb-ssi` | `drivers/spi/spi-dw-mmio.c` (+`spi-dw-core`) | SpbCx | ★★ | 含 2 個 slave 介面 |
| RP1 I2C ×7 | `snps,designware-i2c` | `drivers/i2c/busses/i2c-designware-platdrv.c` | SpbCx | ★★ | DesignWare |
| RP1 PWM ×2 | `raspberrypi,rp1-pwm` | `drivers/pwm/pwm-rp1.c` | KMDF | ★★ | 風扇控制用 |
| RP1 SD/MMC ×2 | `raspberrypi,rp1-dwcmshc` | `drivers/mmc/host/sdhci-of-dwcmshc.c` | sdport miniport | ★★★ | DesignWare MSHC |
| RP1 ADC / 溫度 | `raspberrypi,rp1-adc` | `drivers/hwmon/rp1-adc.c` | KMDF | ★★ | |
| **RP1 PIO** | `raspberrypi,rp1-pio` | `drivers/misc/rp1-pio.c` | KMDF 自訂 | ★★★★ | RP1 獨有可程式 I/O，無對應，需全新 |
| RP1 I2S ×3 | `snps,designware-i2s` | `sound/soc/dwc/dwc-i2s.c`（**未在本次 checkout**） | PortCls 音訊 | ★★★★ | 需另抓 `sound/` |
| RP1 類比音訊輸出 | `raspberrypi,rp1-audio-out` | `sound/soc/...`（**未在本次 checkout**） | PortCls | ★★★ | 需另抓 `sound/` |

### RP1 多媒體（相機 / 顯示輸出）

| 裝置 | DT compatible | Linux 原始碼（已驗證） | Windows 框架 | 難度 | 備註 |
|------|---------------|------------------------|--------------|:----:|------|
| MIPI CSI 相機前端 ×2 | `raspberrypi,rp1-cfe` | `drivers/media/platform/raspberrypi/rp1_cfe/cfe.c` | AVStream 相機 | ★★★★ | 搭配上方 PiSP BE |
| MIPI DSI 顯示 ×2 | `raspberrypi,rp1dsi` | `drivers/gpu/drm/rp1/rp1-dsi/rp1_dsi.c` | WDDM | ★★★★ | |
| DPI 平行顯示 | `raspberrypi,rp1dpi` | `drivers/gpu/drm/rp1/rp1-dpi/rp1_dpi.c` | WDDM | ★★★★ | |
| VEC 類比電視輸出 | `raspberrypi,rp1vec` | `drivers/gpu/drm/rp1/rp1-vec/rp1_vec.c` | WDDM | ★★★ | |

---

## 5. 外接 / 板載晶片

| 裝置 | DT compatible | Linux 原始碼（已驗證） | Windows 框架 | 難度 | 備註 |
|------|---------------|------------------------|--------------|:----:|------|
| **WiFi (CYW43455)** | `brcm,bcm4329-fmac` | `drivers/net/wireless/broadcom/brcm80211/brcmfmac/` | NDIS 原生 802.11 | ★★★★★+ | 走 SDIO2；stack 龐大 |
| **Bluetooth (BCM43438)** | `brcm,bcm43438-bt` | `drivers/bluetooth/hci_bcm.c` | Windows BT（序列 HCI / BthMini） | ★★★★ | 走 uarta |
| Ethernet PHY (BCM54213) | (phy node) | `drivers/net/phy/broadcom.c` | 併入 NDIS miniport | ★ | 屬 Ethernet 一部分 |
| 散熱風扇 (PWM) | `pwm-fan` | `drivers/hwmon/pwm-fan.c` | KMDF + 熱區 | ★ | 接 RP1 PWM1 |
| 電源鍵 | `gpio-keys` | `drivers/input/keyboard/gpio_keys.c` | KMDF HID 按鈕 | ★ | |
| 狀態 LED ×2 | `gpio-leds` | `drivers/leds/leds-gpio.c` | KMDF | ★ | PWR / ACT |
| 電源穩壓 (regulators) | `regulator-fixed`/`-gpio` | `drivers/regulator/` | 多由韌體/ACPI 處理 | ★ | |

---

## 6. 韌體 / 虛擬裝置

| 裝置 | DT compatible | Linux 原始碼 | 備註（Windows 對應） |
|------|---------------|--------------|----------------------|
| RTC（韌體型） | `raspberrypi,rpi-rtc` | `drivers/rtc/` (firmware) | 透過 VC 韌體；可走 ACPI / KMDF |
| **PMIC（DA9090 類）** | *無 DT 節點* | *無 Linux 驅動* | **由 VideoCore VPU 韌體管理**（DT 僅見 `PMIC_INT/SCL/SDA` 腳位）。Windows 端電源管理需評估走 ACPI |
| OTP | `raspberrypi,rpi-otp` | nvmem | 一次性記憶體，通常不需移植 |
| gpiomem / vcio | `raspberrypi,gpiomem` / `vcio` | `drivers/char/broadcom/` | 提供 user space 存取，選擇性 |
| 韌體 KMS | `raspberrypi,rpi-firmware-kms-2712` | `drivers/gpu/drm/vc4/` | 韌體型顯示，與 vc4 相關 |
| VCHIQ（VideoCore 介面） | (vc04_services) | `drivers/staging/vc04_services/` | GPU/音訊/相機的韌體服務通道；HDMI 音訊相關 |

---

## 7. 重點移植難題（必讀）

1. **PCIe 列舉是命脈**：RP1 的所有 I/O 都在 PCIe 底下。先把 `pcie-brcmstb` + RP1 multi-function bus 打通，否則一切免談。
2. **Device Tree → ACPI**：`bcm2712-rpi-5-b.dts` 的所有 `reg`（MMIO 位址）、`interrupts`、`clocks`、`gpios` 關係，都要翻成 ACPI `_CRS` / `_DSD`。這是純手工且容易出錯的工作。
3. **時脈與韌體相依**：大量裝置的 clock/power 由 **VideoCore 韌體（mailbox）** 與 **RP1 韌體** 控制。要先有韌體溝通驅動，其他裝置才能上電/設頻率。
4. **驅動簽章**：Windows ARM64 對驅動簽章要求嚴格。開發階段用 test-signing；正式需 EV 憑證 + 微軟 attestation/WHQL。
5. **ARM64 記憶體模型**：弱記憶體序、cache coherency、MMIO barrier、atomic — 從 Linux 的 `readl/writel/dma_*` 對應到 Windows 的 `READ_REGISTER_*` / `WRITE_REGISTER_*` 與正確 barrier。
6. **標準 IP 是重用機會**：RP1 內 DesignWare（I2C/SPI/DWC3/AXI-DMA/I2S）、Cadence（MACB）、ARM（PL011）皆為業界標準 IP，暫存器有公開文件，移植阻力相對低。
7. **GPU/顯示是另一個世界**：V3D + vc4 顯示管線若要在 Windows 完整支援需 WDDM 全套，工程量等同重做一個 GPU 驅動，建議最後再評估（初期可用基本 framebuffer / 韌體顯示）。
8. **`sound/` 未抓取**：本次只 checkout 了 `drivers/`。I2S 與類比音訊的 machine/codec 驅動在 `sound/soc/`，若要做音訊需另外補抓。

---

## 8. 已確認原始碼路徑索引（快速查找）

```
# Tier 0 開機命脈
drivers/pci/controller/pcie-brcmstb.c          # BCM2712 PCIe RC
drivers/firmware/raspberrypi.c                 # VideoCore 韌體
drivers/firmware/rp1-fw.c                       # RP1 韌體
drivers/mailbox/bcm2835-mailbox.c               # VC mailbox
drivers/mailbox/rp1-mailbox.c                   # RP1 mailbox
drivers/clk/bcm/clk-raspberrypi.c               # VC 韌體 clocks
drivers/clk/clk-rp1.c                           # RP1 clocks
drivers/mmc/host/sdhci-brcmstb.c                # SD 卡
drivers/gpio/gpio-brcmstb.c                     # BCM2712 GPIO
drivers/pinctrl/bcm/pinctrl-brcmstb-bcm2712.c   # BCM2712 pinctrl
drivers/pinctrl/pinctrl-rp1.c                   # RP1 GPIO/pinctrl

# Tier 1 核心 I/O
drivers/net/ethernet/cadence/macb_main.c        # RP1 Ethernet
drivers/usb/dwc3/core.c                          # RP1 USB3
drivers/dma/dw-axi-dmac/dw-axi-dmac-platform.c   # RP1 DMA
drivers/dma/bcm2835-dma.c                        # BCM2712 DMA
drivers/tty/serial/amba-pl011.c                  # PL011 UART (BCM2712 + RP1)

# Tier 2 周邊匯流排
drivers/i2c/busses/i2c-designware-platdrv.c      # RP1 I2C
drivers/i2c/busses/i2c-brcmstb.c                 # BCM2712 I2C
drivers/i2c/busses/i2c-bcm2835.c                 # BCM2712 I2C(2835)
drivers/spi/spi-dw-mmio.c                         # RP1 SPI
drivers/spi/spi-bcm2835.c                         # BCM2712 SPI
drivers/pwm/pwm-rp1.c                             # RP1 PWM
drivers/hwmon/rp1-adc.c                           # RP1 ADC
drivers/mmc/host/sdhci-of-dwcmshc.c              # RP1 SD/MMC
drivers/misc/rp1-pio.c                            # RP1 PIO (獨有)

# Tier 3 多媒體
drivers/gpu/drm/v3d/v3d_drv.c                     # V3D GPU
drivers/gpu/drm/vc4/vc4_hdmi.c                    # HDMI
drivers/gpu/drm/vc4/vc4_hvs.c                     # 顯示合成
drivers/gpu/drm/rp1/rp1-dsi/rp1_dsi.c             # MIPI DSI
drivers/gpu/drm/rp1/rp1-dpi/rp1_dpi.c             # DPI
drivers/gpu/drm/rp1/rp1-vec/rp1_vec.c             # 類比電視
drivers/media/platform/raspberrypi/rp1_cfe/cfe.c        # 相機前端
drivers/media/platform/raspberrypi/pisp_be/pisp_be.c    # ISP
drivers/media/platform/raspberrypi/hevc_dec/hevc_d.c    # HEVC 解碼
drivers/iommu/bcm2712-iommu.c                     # IOMMU

# Tier 4 無線
drivers/net/wireless/broadcom/brcm80211/brcmfmac/ # WiFi
drivers/bluetooth/hci_bcm.c                        # Bluetooth

# 其他 SoC
drivers/char/hw_random/iproc-rng200.c             # RNG
drivers/thermal/broadcom/bcm2711_thermal.c        # 溫度
drivers/pmdomain/bcm/raspberrypi-power.c          # power domain
drivers/usb/dwc2/                                  # BCM2712 USB OTG
drivers/tty/serial/8250/8250_bcm7271.c            # AON UART
```

---

## 9. 統計摘要

- **BCM2712 SoC 平台裝置**：約 22 項（含多媒體 7 項）
- **RP1 南橋裝置**：約 19 項（含多媒體 4 項）
- **外接/板載晶片**：7 項
- **韌體/虛擬裝置**：6 項
- **最高難度（建議最後做）**：V3D GPU、vc4 顯示管線、WiFi 802.11、相機/ISP
- **重用機會最高**：RP1 內 DesignWare / Cadence / PrimeCell 標準 IP

> 下一步建議：先鎖定 **Tier 0**（PCIe + 韌體 + clock + GPIO + SD），確立 ACPI 描述與韌體溝通機制，再逐 Tier 推進。
