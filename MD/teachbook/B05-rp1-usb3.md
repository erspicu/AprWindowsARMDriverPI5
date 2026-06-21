# B5：RP1 USB3 ×2（Synopsys DWC3，inbox xHCI）

> 又一個「**先借 inbox**」的範例——DWC3 對外是標準 xHCI，Windows 自帶 xHCI 驅動。

| | |
|---|---|
| **裝置** | RP1 USB3 ×2（Synopsys DesignWare DWC3） |
| **Linux 源碼** | `drivers/usb/dwc3/core.c` |
| **Windows 框架** | inbox **xHCI**（USBXHCI.sys）— xHCI 相容性待查 |
| **本專案** | ACPI 描述為 `PNP0D10`（inbox xHCI），🔲 |

## 1. DWC3 = xHCI 相容控制器

DWC3 是 host/device 雙模 USB3 控制器，**host 模式對外是標準 xHCI 介面**。Windows 的 inbox
`USBXHCI.sys` 接管 xHCI——所以理想情況：ACPI 描述成 xHCI（`PNP0D10`）+ 正確中斷，就能借 inbox 驅動。
```asl
Device (USB0) { Name (_HID, "PNP0D10") Name (_UID, 0)     // inbox xHCI
    Method (_CRS) { ... GpioInt(...) { 31 } ... } }
```

## 2. 但 DWC3 有「核心初始化」要先做

DWC3 上電後、進 xHCI 模式前，往往要做一段 **core init / PHY 設定 / 模式選擇（設成 host）**。
這段在 Linux 是 `dwc3_core_init()`。在 Windows：
- 若 UEFI/韌體已把 DWC3 設成 host 模式並初始化好 → inbox xHCI 直接接管，最省事。
- 若沒有 → 可能要一個薄 KMDF「pre-init」驅動先設好 DWC3 core，再交給 xHCI。

## 3. Pi5 實測

- 兩顆 USB3 在 RP1 BAR1 `0x200000`/`0x300000`，IRQ **31 / 36**（Pi5 實測 `xhci-hcd:usb1/usb3`）。
  > 註：本專案曾把 IRQ 估成 30/35，實機校正為 31/36（A 類）。

## 本章重點

- DWC3 host 對外是 **xHCI** → 優先借 inbox `USBXHCI`。
- 但要先確認 **DWC3 core/PHY/模式**已初始化（韌體做或薄驅動做）。
- IRQ 31/36 為 Pi5 實測校正值。

➡️ 回 [目錄](README.md)
