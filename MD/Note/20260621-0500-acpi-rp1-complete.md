# ACPI rp1.asl 補完（全 RP1 子裝置）— ✅ x64 完整完成

> 日期：2026-06-21（loop 第 1 輪）｜ `windows_sources/pcie-rp1/acpi/rp1.aml`（asl.exe 編譯過, 3393 B）

## 內容
- `PNP0A08` host bridge（_CRS = `WordBusNumber` 0–0xFF；記憶體窗由韌體 MCFG 出）。
- `RP1`（_ADR dev1/func0）下補全 **~33 個子裝置**，每個 `_HID`+`_UID`+`_CRS(GpioInt=RP1 內部 IRQ，指向 RP1 GpioClx 控制器節點)`：
  - UART0–5 → `ARMH0011`(inbox) IRQ 25/42/43/44/45/46
  - I2C0–6 → `RPI50001` IRQ 7–13（+`_DSD` clock-frequency 100kHz）
  - SPI0–5 → `RPI50002` IRQ 19–24
  - I2S0–2 → `RPI50003` IRQ 14–16；audio_out → `RPI50004` IRQ 4
  - PWM0/1 → `RPI50005` IRQ 5/41
  - GPIO → `RPI50006` IRQ 0/1/2（3 banks，**拆成 3 個 GpioInt**）
  - ETH0 → `RPI50007` IRQ 6
  - CSI0/1 → `RPI50008` IRQ 47/48
  - SD0/1 → `RPI50009` IRQ 17/18
  - DMA0 → `RPI5000A` IRQ 40
  - USB0/1 → `PNP0D10`(inbox xHCI) IRQ 30/35

## asl.exe 踩雷
- **每個 `GpioInt` 描述子只能放一條中斷**（`asl_ERR: only one interrupt per GPIO interrupt descriptor supported`）。多 IRQ（如 GPIO 3 banks）要寫成多個 `GpioInt {...}`。
- 編譯器：`...\Tools\10.0.26100.0\x64\ACPIVerify\asl.exe`（ACPI 5.0）。

## HID ↔ 驅動對照（INF 要對齊）
RPI50001=rp1i2c / 50002=rp1spi / 50003=rp1i2saud / 50004=audio / 50005=rp1pwm / 50006=rp1gpio / 50007=rp1gem / 50008=rp1cfe / 50009=rp1sd / 5000A=rp1dma。

## 註
- 子裝置 MMIO 不在 ACPI（由 `rp1bus.sys` 切 BAR1 注入）；ACPI 只供 GpioInt + _DSD。
- 實際綁定可能走 bus driver 的 HWID `RP1\<name>`（`WdfPdoInitAssignAcpiName` 連此 ACPI 取 GpioInt）。
- **下一輪 loop：各驅動 INF**（HID 對齊上表）。
