# A10：UART（ARM PL011，SerCx2 / inbox）

> 教「**能用 inbox 驅動就別自己寫**」——PL011 是標準 ARM PrimeCell，Windows 自帶驅動。
> （RP1 的 6 個 UART [B7] 是 PL011 的 AXI 變體，概念相同。）

| | |
|---|---|
| **裝置** | BCM2712 debug UART（PL011）+ RP1 UART ×6（PL011-AXI 變體） |
| **Linux 源碼** | `drivers/tty/serial/amba-pl011.c` |
| **Windows 框架** | **SerCx2**；PL011 有 **inbox `SerPl011.sys`** |
| **本專案** | UART 直接用 inbox SerPl011（ARMH0011） |

## 1. 重要決策：先找 inbox 驅動

Windows 對常見標準 IP 自帶驅動。PL011 就有 **inbox `SerPl011.sys`**，用 ACPI ID **`ARMH0011`** 即可綁定——
**完全不用自己寫 UART 驅動**。
```asl
Device (URT0) { Name (_HID, "ARMH0011") Name (_UID, 0)
    Method (_CRS) { ... GpioInt(...) { 25 } ... } }   // 綁 inbox SerPl011
```

> 教學點：移植第一步永遠先問「**Windows 有沒有 inbox 驅動能用？**」。UART(PL011)、USB(xHCI)、
> 部分 SD 都可借力，省下整個驅動。能借就借，把力氣留給沒得借的（V3D/PIO/RP1bus…）。

## 2. 何時還是得自己寫？

- RP1 的 PL011 是 **AXI 變體**，若 inbox 驅動對某些暫存器行為不相容，可能要寫薄 shim 或自訂 SerCx2 driver。
- 但起手式一定先試 inbox：`ARMH0011` + 正確的 `_CRS`（中斷 + 時脈）。

## 3. ACPI / 時脈

- RP1 UART 時脈：`clk_uart = 50 MHz`（Pi5 實測）；BCM debug UART 時脈 44.2368 MHz。
- 這些頻率要在 ACPI/`_DSD` 或驅動裡告訴 UART 驅動算波特率。

## 本章重點

- PL011 用 **inbox `SerPl011`（`ARMH0011`）**，多半不用自己寫。
- 移植 SOP 第一步：**先找 inbox 驅動**能不能借。
- RP1 的 AXI 變體若不相容才考慮自寫；時脈值用實機量。

➡️ 回 [目錄](README.md)
