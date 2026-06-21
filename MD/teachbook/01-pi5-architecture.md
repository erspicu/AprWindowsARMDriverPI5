# 第 01 章：Pi5 是什麼？雙晶片架構總覽（BCM2712 + RP1）

> 目標：在動手寫任何驅動前，先搞懂 Pi5 的硬體長相——尤其「兩顆晶片」的分工，以及為什麼這件事決定了整個移植的順序。

## 1.1 一句話版本

Raspberry Pi 5 不是「一顆 SoC 包辦全部」，而是 **兩顆晶片**：

- **BCM2712** — 主 SoC：4× Cortex-A76 CPU、GPU(VideoCore VII / V3D)、顯示輸出、記憶體控制器、開機用的 SD/eMMC、以及由 **VideoCore 韌體**管理的時脈/電源。
- **RP1** — 樹莓派自製的 **I/O 南橋**：USB、Gigabit Ethernet、GPIO、UART、SPI、I2C、PWM、ADC、I2S、相機 CSI…幾乎所有「對外 I/O」都在這顆。

兩顆之間用 **PCIe Gen2 ×4** 連接。

```
        +-------------------- BCM2712 SoC --------------------+
        |  Cortex-A76 ×4   GIC-400   VideoCore VII GPU(V3D)   |
        |  HDMI ×2  HVS  HEVC dec  ISP(PiSP)  IOMMU  DMA      |
        |  SD/eMMC(SDHCI)  I2C  SPI  PL011  RNG  Mailbox      |
        |  VideoCore 韌體 (管 clock / power / PMIC)            |
        +------------------------+----------------------------+
                                 | PCIe Gen2 ×4
                                 v
        +-------------------- RP1 南橋 -----------------------+
        |  USB3 ×2(dwc3)   Gigabit Ethernet(Cadence GEM)     |
        |  GPIO/pinctrl   UART ×6   SPI ×8   I2C ×7   PWM ×2 |
        |  I2S ×3   類比音訊   ADC   SD/MMC   PIO            |
        |  MIPI CSI ×2(相機)   DSI/DPI/VEC(顯示輸出)         |
        |  RP1 韌體 / mailbox   clocks                       |
        +----------------------------------------------------+
```

## 1.2 為什麼「兩顆晶片」對移植這麼重要？

因為 **RP1 的所有暫存器都在 PCIe 的位址空間（BAR）裡**。作業系統一開機**看不到 RP1 的任何 I/O**，
除非先做到三件事：

1. **BCM2712 的 PCIe Root Complex** 能運作（UEFI/ACPI 通常已處理 ECAM）。
2. PCIe **列舉**出 RP1 這個 endpoint（`VEN_1DE4 & DEV_0001`）。
3. 有一個 **bus driver** 把 RP1 內部的眾多周邊，當成「子裝置」分出來給各自的驅動。

所以本專案有句話：**「PCIe 是命脈」**。沒打通 PCIe + RP1 列舉，USB/網路/GPIO/I2C… 通通看不到。
（細節見 [第 05 章](05-pcie-and-rp1-enumeration.md)。）

## 1.3 移植要分開看的兩套位址

| | BCM2712 SoC 周邊 | RP1 周邊 |
|---|---|---|
| 位址 | 直接在 SoC 匯流排（如 `0x107D...`） | 在 PCIe **BAR1** 內（實體 `0x1F00000000`，4MB） |
| 中斷 | 直接掛 **GIC**（GSIV，如 305） | RP1 內部 IRQ（0–60）→ **MSI-X**（61 條）→ GIC |
| 在 Windows 怎麼來 | ACPI 直接描述 + inbox/各 class driver | 靠 `rp1bus.sys` 切 BAR1 + demux MSI-X |
| 範例裝置 | SD/eMMC、debug UART、主 GPIO | USB、Ethernet、40-pin 上的 GPIO/I2C/SPI |

> **常見陷阱**：Pi5 的 SD 卡 / eMMC 其實在 **BCM2712**（不是 RP1）；而 40-pin 排針上的 GPIO/I2C/SPI 在 **RP1**。
> 移植時搞錯歸屬，位址與中斷就全錯。（本專案就修正過 SDHCI 被誤當 RP1 子裝置的 bug。）

## 1.4 「韌體」是隱形的老大

很多裝置的**時脈與電源不是 CPU 直接控制**，而是由：

- **VideoCore 韌體**（跑在 BCM2712 的 VPU 上）— 管 clock/power/溫度/PMIC，透過 **mailbox** 溝通。
- **RP1 韌體** — 管 RP1 內部的部分設定。

意思是：你想把某個周邊「上電、設頻率」，往往得**先有韌體溝通驅動**（mailbox）才能做到。
這也是為什麼移植優先序裡，韌體/mailbox/clocks 屬於最早的 **Tier 0**。

## 1.5 移植優先序（先有命脈，再有周邊）

| 階段 | 內容 | 為何先做 |
|------|------|----------|
| **Tier 0 命脈** | PCIe RC、VideoCore/RP1 韌體+mailbox、clocks、GIC(ACPI)、SD(開機)、GPIO | 沒這些開不了機、看不到 RP1 |
| **Tier 1 核心 I/O** | Ethernet、USB3、除錯 UART、DMA | 基本可用系統 |
| **Tier 2 周邊匯流排** | I2C、SPI、PWM、ADC、RTC、RP1 MMC | 一般周邊 |
| **Tier 3 多媒體** | HDMI/顯示、V3D GPU、相機/ISP、I2S 音訊 | 工程量最大 |
| **Tier 4 無線** | WiFi(SDIO)、Bluetooth(UART) | stack 龐大 |

## 1.6 重用機會：RP1 大量用「業界標準 IP」

好消息：RP1 內很多周邊不是樹莓派自己發明的，而是**業界標準 IP**，暫存器有公開文件、Linux 驅動成熟：

- **Synopsys DesignWare**：I2C、SPI(SSI)、I2S、USB(DWC3)、AXI-DMA
- **Cadence**：Gigabit Ethernet（GEM / macb）
- **ARM PrimeCell**：UART（PL011）

這些「可沿用暫存器定義與初始化序列」的部分，正是移植阻力最低、最適合入門的起點
（本書第二部 B 系列多從這裡切入）。

## 本章重點

- Pi5 = **BCM2712（主 SoC）+ RP1（I/O 南橋）**，用 PCIe 連接。
- **RP1 全部 I/O 在 PCIe BAR 內** → 「打通 PCIe 列舉」是移植命脈。
- 兩套位址/中斷要分開看；**SD/eMMC 在 BCM2712，40-pin I/O 在 RP1**。
- 時脈/電源常由**韌體（mailbox）**管 → Tier 0 要先做。
- RP1 多為**標準 IP**（DesignWare/Cadence/PrimeCell）→ 重用機會高，適合入門。

➡️ 下一章：[Linux vs Windows 驅動模型的根本差異](02-linux-vs-windows-driver-model.md)
