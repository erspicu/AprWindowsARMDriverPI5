# 第 04 章：硬體描述——從 Device Tree 到 ACPI

> 目標：學會把 Linux 的 **Device Tree** 節點，翻譯成 Windows 認得的 **ACPI (ASL)** 描述。
> 這是純手工、容易出錯，但**不可跳過**的工作——驅動再好，裝置沒被 ACPI 描述出來就不會載入。

## 4.1 兩者在做同一件事

| | Device Tree (Linux) | ACPI (Windows) |
|---|---|---|
| 角色 | 告訴 OS「有哪些硬體、在哪、用哪個 IRQ/clock」 | 同上 |
| 格式 | `.dts`（編譯成 `.dtb`） | `.asl`（編譯成 `.aml`） |
| 比對驅動 | `compatible` 字串 | `_HID`（硬體 ID） |
| 資源 | `reg`/`interrupts`/`clocks`/`gpios` | `_CRS`（`Memory`/`Interrupt`/`GpioInt`） |
| 額外屬性 | 自訂 property | `_DSD`（device-specific data） |

## 4.2 對照一個真實節點

**Device Tree（RP1 的一個 I2C）：**
```dts
i2c@70000 {
    compatible = "snps,designware-i2c";
    reg = <0x70000 0x1000>;          // 在 RP1 內的偏移
    interrupts = <7>;                 // RP1 內部 IRQ 7
    clocks = <&rp1_clocks ...>;
};
```

**ACPI（本專案 `windows_sources/pcie-rp1/acpi/rp1.asl`）：**
```asl
Device (I2C0) {
    Name (_HID, "RPI50001")           // 對應 INF 綁定的硬體 ID
    Name (_UID, 0)
    Method (_CRS) { Name (RB, ResourceTemplate () {
        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0,
                 "\\_SB.PCI0.RP1") { 7 }    // RP1 內部 IRQ 7
    }) Return (RB) } }
```

對應關係：
- `compatible` → **`_HID`**（自訂一組 vendor ID，例 `RPI5xxxx`），再由 INF 把驅動綁上去。
- `interrupts = <7>` → **`GpioInt { 7 }`**，且指明中斷來源是 RP1（`\_SB.PCI0.RP1`）。
- `reg`（MMIO）→ RP1 子裝置的 MMIO **不在 `_CRS`**，而是由 **bus driver（`rp1bus.sys`）切 BAR1 後分配**
  （見 [第 05 章](05-pcie-and-rp1-enumeration.md)）。

## 4.3 RP1 子裝置 vs BCM2712 SoC 裝置：兩種 _CRS 寫法

**RP1 子裝置**（中斷走 RP1 內部 IRQ → 用 `GpioInt`，MMIO 由 bus driver 給）：
```asl
Device (DMA0) { Name (_HID, "RPI5000A") Name (_UID, 0)
    Method (_CRS) { Name (RB, ResourceTemplate () {
        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 40 }
    }) Return (RB) } }
```

**BCM2712 SoC 裝置**（固定 MMIO + GIC 中斷 → 用 `QWordMemory` + `Interrupt`）：
```asl
Device (SDC0)                         // eMMC（在 BCM2712，不是 RP1！）
{
    Name (_HID, "RPI50009") Name (_UID, 0)
    Method (_CRS, 0, NotSerialized) { Name (RB, ResourceTemplate () {
        // 真實實體位址（Pi5 實測 /proc/iomem），>4GB 需用 QWordMemory
        QWordMemory (ResourceConsumer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
            0x0, 0x0000001000FFF000, 0x0000001000FFF5FF, 0x0, 0x0000000000000600)
        Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 305 }   // GIC GSIV
    }) Return (RB) } }
```

> **GSIV 怎麼算**：GIC 的 SPI 中斷，GSIV = Device-Tree 的 SPI 號 + 32。
> 例：DT `interrupts = <0 273 4>` → GSIV = 273 + 32 = **305**。經 L2 級聯的，取 L2 控制器的 GIC 線。

## 4.4 PCIe Root Complex：用內建 ID

PCIe RC 不用自訂驅動，用 ACPI 標準 ID，讓 inbox `pci.sys` 接手：
```asl
Device (PCI0) {
    Name (_HID, EisaId ("PNP0A08"))   // PCI Express root bridge
    Name (_CID, EisaId ("PNP0A03"))   // 相容 PCI root bridge
    Name (_SEG, Zero)
    Name (_BBN, Zero)
    Method (_CRS) { ... WordBusNumber(...) ... }   // bus 範圍；ECAM 由 MCFG 表提供
    Device (RP1) { Name (_ADR, 0x00010000) ... }    // RP1 = PCI device 1, function 0
}
```

## 4.5 編譯與驗證

```powershell
# ASL → AML（WDK ACPIVerify 內附 asl.exe）
asl.exe windows_sources\pcie-rp1\acpi\rp1.asl      # 產出 rp1.aml
```
編譯通過會印 `Compliant with the ACPI 5.0 Specification` + image size。

> 常見錯誤：`only one interrupt per GPIO interrupt descriptor` → 一個 `GpioInt {}` 只能放一條 IRQ，
> 多條要拆成多個 `GpioInt {a}` `GpioInt {b}`。

## 4.6 翻譯流程小結（SOP）

1. 在 Linux DT（`bcm2712-rpi-5-b.dts` / RP1 overlay）找到裝置節點。
2. 判斷它是 **RP1 子裝置**還是 **BCM2712 SoC 裝置**（決定 `_CRS` 寫法）。
3. 取 `reg`（位址/偏移）、`interrupts`（IRQ）、必要的 `clocks`/`gpios`。
4. **位址/IRQ 的「絕對值」要用實機校正**（DT 只有偏移；絕對位址/GSIV 是 runtime 才定的——見
   [第 08 章](08-hardware-truth-and-ab-gaps.md) 的「A 類」）。
5. 配一組 `_HID`，寫進 `rp1.asl`，並在對應 INF 綁 `ACPI\<_HID>`。
6. `asl.exe` 編譯 + `infverif` 驗 INF。

## 本章重點

- DT 與 ACPI 做同一件事：描述硬體。`compatible`→`_HID`、`reg/interrupts`→`_CRS`。
- **RP1 子裝置**用 `GpioInt`（MMIO 由 bus driver 給）；**BCM2712 SoC 裝置**用 `QWordMemory`+`Interrupt`。
- **GSIV = DT SPI 號 + 32**（經 L2 取 L2 的 GIC 線）。
- 絕對位址/IRQ **要用實機校正**，別只照 DT 偏移猜。

➡️ 下一章：[命脈：PCIe 與 RP1 列舉](05-pcie-and-rp1-enumeration.md)
