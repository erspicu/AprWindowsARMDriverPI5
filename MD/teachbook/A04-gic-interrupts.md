# A4：GIC-400 / L2 中斷控制器（ACPI，不寫驅動）

> 教「**有些硬體不用寫驅動，寫對 ACPI 表就好**」——中斷控制器是最典型的例子。

| | |
|---|---|
| **裝置** | ARM GIC-400（主中斷控制器）+ brcmstb L2 中斷控制器（級聯） |
| **Linux 源碼** | `drivers/irqchip/irq-gic.c`、`irq-brcmstb-l2.c` |
| **Windows 框架** | **ACPI MADT（OS 內建）** + 級聯描述 |
| **本專案** | ACPI 描述（➖ 免驅動） |

## 1. GIC：寫 ACPI MADT，不寫 .sys

ARM64 的 GIC 由 **Windows 核心內建支援**——你不寫驅動，只要在 ACPI 的 **MADT（Multiple APIC
Description Table）** 把 GIC 的分佈/CPU 介面/redistributor 描述對，OS 就會用它。
（這通常是平台 UEFI 韌體提供的，移植 RP1 周邊時你主要是「填對各裝置的 GSIV」。）

## 2. GSIV：裝置怎麼指到 GIC 中斷

裝置在 `_CRS` 用 `Interrupt(){ GSIV }` 指定中斷號。**GIC SPI 的 GSIV = DT 的 SPI 號 + 32**：
```
DT: interrupts = <0 273 4>      →  GSIV = 273 + 32 = 305
```

## 3. 級聯（L2 intc）：比較麻煩的部分

有些裝置的中斷不直接掛 GIC，而是先進 **brcmstb L2 中斷控制器**，再由 L2 彙整成一條 GIC 線。
例（BCM2712 GPIO，見 [A6](A06-bcm2712-gpio.md)）：
```
gpio.irq → L2-intc@7d508400 → GIC 244 → GSIV 276
```
ACPI 描述這種裝置時，`Interrupt` 要填 **L2 控制器在 GIC 上那條線換算的 GSIV**（不是裝置在 L2 上的 pin）。
> Windows 對級聯 L2 的支援有限——常見做法是把「經 L2 的那群裝置」都指到 L2 的 GIC 線，由一個驅動再 demux
> （就像 [rp1bus 用 GpioClx demux RP1 中斷](A01-pcie-root-complex.md) 那樣）。

## 4. 同類「免驅動、寫 ACPI」的還有

| 硬體 | ACPI 表 |
|------|---------|
| Arch timer / 系統計時器 | **GTDT** |
| PCIe ECAM | **MCFG** |
| GIC | **MADT** |

## 本章重點

- 中斷控制器/timer/ECAM **不寫驅動**，寫對 ACPI 表（MADT/GTDT/MCFG）。
- 裝置中斷用 `Interrupt(){GSIV}`；**GSIV = DT SPI + 32**。
- 級聯 L2 較麻煩：填 L2 的 GIC 線 GSIV，必要時用一個驅動再 demux。

➡️ 回 [目錄](README.md)
