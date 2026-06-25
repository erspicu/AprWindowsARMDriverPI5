# Pi5 社群 UEFI + 我們驅動的整合策略（2026-06-25）

> 來源：WebSearch（worproject repos）+ Gemini 諮詢（`tools/knowledgebase/message/20260625_222647.md`）。
> 核心問題：我們只寫 Windows 驅動、不寫韌體，如何接上社群 Pi5 UEFI 讓裝置在 Win11-ARM 列舉。

## 社群 repo（確定）
- **`worproject/rpi5-uefi`** = binary release + wrapper（預編 `RPI_EFI.fd`、TF-A、`config.txt`、打包腳本）。**不要 fork 改 code。** 放 SD 卡開機引導用。
- **`worproject/edk2-platforms`** = **source**（ACPI ASL + 底層 C 驅動）。**要自訂韌體就 fork 這個**，搭 `tianocore/edk2` + `edk2-non-osi` 一起 build。
- Build 環境：**WSL2 Ubuntu**；`build -a AARCH64 -t GCC5 -p Platform/RaspberryPi/RPi5/RPi5.dsc -b RELEASE` → 產出 `Build/RPi5/.../FV/RPI_EFI.fd`。
- ⚠️ **已知限制（需查證對 RP1 的影響）**：社群韌體目前 **PCIe 只支援 single-function device**、PL011 UART 有問題、SD 限 DDR50。RP1 若以多 function 呈現，這條要先確認。

## Windows 列舉模型（確定）
- **WoA 絕對靠 ACPI**（`acpi.sys` 解析 DSDT/SSDT），**Device Tree 在 Windows 無效**。
- ACPI 表在 edk2-platforms 的 `Silicon/Broadcom/Bcm2712/AcpiTables/` 或 `Platform/RaspberryPi/AcpiTables/`（找 `Dsdt.asl` 及附屬 `.asl`）。

## 兩類裝置、兩種策略（這是作戰核心）
### RP1 底下周邊（GPIO/I2C/SPI/UART/PWM…，走 PCIe）→ 做法 B：KMDF Bus Driver
- 寫一個 KMDF 驅動 match RP1 PCIe 的 `PCI\VEN_xxxx&DEV_xxxx`（**VID/PID 需實機查證**），map BAR，依 RP1 內部 offset 用 `WdfChildListCreate`/`WdfPdoInitAllocate` **動態建立子 PDO**，給自訂 HWID（如 `RP1Bus\GPIO`、`RP1Bus\I2C0`），再各自寫驅動 match。
- ✅ **完全不動韌體 ACPI**，全在 Windows kernel C 控制 → **符合「開機後 pnputil 後裝」**。
- 代價：接在 RP1 I2C 上的感測器無法用 ACPI 標準 SPB descriptor（`I2cSerialBusV2`）reference，得由 bus driver 自行 spawn/提供介面。
- → **正對應我們已有的 `pcie-rp1/rp1bus` 骨架**；策略選定＝做法 B。

### BCM2712 SoC 直連（mailbox/RNG/watchdog/HVS 顯示…，純 MMIO）→ 必須 ACPI
- 沒有 PCI/USB config space，Windows 不知道那有裝置 → **必須用 ACPI `Memory32Fixed` + `Interrupt` 描述**。
- **開發期捷徑（救星，不必重 build/重刷 SD）**：`asl.exe /loadtable ssdt.aml` 動態注入 SSDT：
  1. 寫只含新增 SoC 裝置的 `xxx.asl`（`Scope(\_SB){ Device(RNG0){ Name(_HID,"RPI50002") ... Memory32Fixed(...) }}`）。
  2. WDK `asl.exe xxx.asl` 編成 `.aml`。
  3. Pi5 上（admin）`asl.exe /loadtable ssdt.aml` → 重開機 → 裝置管理員出現未知裝置(HWID `RPI50002`) → `pnputil` 裝我們的驅動。
- 最終階段才把這些 ASL 併回 fork 的 `Dsdt.asl` 重 build 一版正式 `RPI_EFI.fd`。

## 哪些純後裝 vs 必須改韌體（確定）
- **純後裝（pnputil，不動韌體）**：USB 全部；標準 PCIe（含 RP1 本身，只要 UEFI PCIe RootPort 正常 Windows 就掃得到）；經我們 bus driver 動態列出的子裝置。
- **必須 ACPI（或開發期 SSDT 注入）**：BCM2712 SoC 內建 MMIO 模組（mailbox/RNG/WDT/HVS 等）。

## 我們的作戰計畫（結論）
1. **RP1 周邊** → 做法 B（KMDF PCIe bus driver 動態列舉）→ 純後裝，**零韌體改動**。
2. **BCM2712 SoC 模組** → 開發期用 `asl.exe /loadtable` SSDT 注入 → 後裝驗證；定案後再併進 fork 的 edk2-platforms `Dsdt.asl` rebuild。
3. 我們現有的 `pcie-rp1/acpi/rp1.aml` ＝ 第 2 類的 SSDT/DSDT 內容來源；可直接拿去 `asl.exe /loadtable` 走捷徑驗證。

## 待查證
- RP1 PCIe 的真實 VEN/DEV ID；社群韌體 single-function PCIe 限制是否擋住 RP1 多 function。
- edk2-platforms RPi5 branch 的確切 AcpiTables 路徑與 build manifest。
