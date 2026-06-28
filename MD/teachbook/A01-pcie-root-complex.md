# A1：PCIe Root Complex 與 RP1 列舉（命脈）

> 整個移植最關鍵的一章——沒有它，RP1 底下所有 I/O 都看不到。觀念基礎見 [第 05 章](05-pcie-and-rp1-enumeration.md)，
> 本章聚焦「要寫哪些東西」。
>
> ⚠️ **策略更新（2026-06）**：RP1 子裝置的列舉現以 **做法 A＝客製 UEFI ACPI 描述為主線**（見 [第 11 章](11-custom-uefi-firmware.md)）——
> UEFI 直接在 ACPI 描述 RP1 周邊，子裝置免 bus driver 即列舉。本章的 **`rp1bus.sys`（做法 B：KMDF bus driver 動態列舉）降為備案**
> （適用「不能改韌體」情境，Pi5 上不存在）。**PCIe RC 本身仍是命脈**（不論哪條路都要先打通 PCIe）。兩法的中斷 demux 都仍需驅動。

| | |
|---|---|
| **裝置** | BCM2712 PCIe Root Complex + RP1 endpoint |
| **Linux 源碼** | `drivers/pci/controller/pcie-brcmstb.c` |
| **Windows 框架** | inbox `pci.sys`（ECAM）+ 自製 KMDF **bus driver** |
| **本專案** | `windows_driver/pcie-rp1/rp1bus.sys` + `windows_sources/pcie-rp1/acpi/rp1.asl` ｜ 🟢 骨架 |

## 1. 兩段要分開做

1. **PCIe RC 本身**：Windows-on-ARM 的 UEFI 韌體通常已透過 **ACPI MCFG（ECAM）+ PNP0A08** 把 PCIe 跑起來，
   inbox `pci.sys` 就能列舉到 RP1 endpoint（`1de4:0001`）。→ 這段**多半不用寫驅動，寫對 ACPI 即可**。
2. **RP1 multi-function bus driver（要寫）**：把 RP1 這個「單一 endpoint」內部的幾十個周邊**分出來**。

## 2. rp1bus.sys 做什麼（四件事）

```
rp1bus.sys（KMDF bus driver，綁 VEN_1DE4&DEV_0001）
├─ 1. map BAR1（4MB MMIO 視窗，實體 0x1F00000000）
├─ 2. 為每個 RP1 內部周邊建「子 PDO」，分配它在 BAR1 裡的 MMIO 區段
├─ 3. 註冊 GpioClx，把 RP1 的 61 條內部 IRQ（經 MSI-X）做 demux
└─ 4. 子裝置以 ACPI GpioInt(pin=RP1 內部 IRQ) 連回這裡
```

## 3. 子 PDO 的 MMIO 分配（概念 code）

```c
// 一張 RP1 周邊偏移表（Pi5 實測 /proc/iomem 校正）
static const RP1_CHILD g_children[] = {
    { "RPI50001", 0x070000, 0x1000, /*irq*/ 7  },  // I2C0
    { "RPI50006", 0x0d0000, 0x30000,/*irq*/ 0  },  // GPIO（3 bank）
    { "RPI50007", 0x100000, 0x10000,/*irq*/ 6  },  // Ethernet
    { "RPI5000A", 0x188000, 0x1000, /*irq*/ 40 },  // DMA
    // ...
};
// 為每個 child 建 PDO，記憶體資源 = BAR1_phys + offset, length
```

> 偏移/IRQ 的**絕對值要用實機校正**：BAR1 絕對位址（PCIe 列舉時定）、各周邊偏移、MSI-X 數，
> 都用 Pi5 `lspci`/`iomem` 量過（A 類，見 [第 08 章](08-hardware-truth-and-ab-gaps.md)）。

## 4. 為什麼先做這個

| 沒有 rp1bus | 有了 |
|---|---|
| 裝置管理員只看到一張「不認識的 PCIe 卡」 | RP1 底下周邊各自列舉、載入驅動 |
| 任何 RP1 驅動都無從綁定 | 子裝置拿得到 MMIO、中斷接得上 |

→ **Tier 0 的 Tier 0**。本書第二部 B 系列所有 RP1 裝置，都站在這章肩膀上。

## 5. 狀態

骨架已建（map BAR1、列舉子 PDO、GpioClx demux）；真正的 PCIe 列舉 + MSI-X demux 在實機行為需 Pi5 + WoA 驗。

➡️ 回 [目錄](README.md)
