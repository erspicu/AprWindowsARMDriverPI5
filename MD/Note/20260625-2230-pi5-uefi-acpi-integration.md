# Pi5 社群 UEFI + 我們驅動的整合策略（2026-06-25）

> 來源：WebSearch（worproject repos）+ Gemini 諮詢（`tools/knowledgebase/message/20260625_222647.md`）。
> 核心問題：我們只寫 Windows 驅動、不寫韌體，如何接上社群 Pi5 UEFI 讓裝置在 Win11-ARM 列舉。

## ★ 決策（2026-06-26）：主線 = 客製 UEFI ACPI（做法 A）
- **做法 A（客製 UEFI ACPI）＝主線**。理由：**Pi5 要跑 Windows 本來就一定得刷 worproject 客製 UEFI**（無官方 Windows-on-Pi5 韌體），既然韌體刷定了，把 RP1 周邊 ACPI 塞進去**零額外部署成本**，且裝置走標準框架（SpbCx/GpioClx/SerCx2）、I2C 感測器能用 `I2cSerialBusV2` reference 控制器。已實作並 build 成功（`uefi_fixed/` + `uefi_build/RPI_EFI.fd`）。
- **做法 B（`windows_sources/pcie-rp1/rp1bus` KMDF bus driver）＝降為備案**。唯一勝場是「stock 韌體、不准改 UEFI」——但 Pi5 沒有 stock Windows 韌體，此情境不存在。保留當參考。
- **共同未解（兩法都要驅動處理）**：RP1 所有子裝置共用一條中斷（`RP1B.PINT`=PCIE2 INTA# 261）→ per-device 中斷 demux 仍需 RP1 內部中斷控制器驅動（GpioClx 角色）。ACPI 只給 MMIO + 一條共享 IRQ。

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

---

## 實作：rp1bus 補成可動態列舉的 KMDF Bus Driver（2026-06-25 完成）
依「做法 B」把 `windows_sources/pcie-rp1/rp1bus/` 骨架補完，ARM64 `/kernel` 編譯 + link 乾淨（`rp1bus.sys` 19KB）。

### 架構（已可動態列舉）
1. **bind PCI**：INF match `PCI\VEN_1DE4&DEV_0001`（RP1 C0），System class，後裝 `pnputil /add-driver rp1bus.inf /install`。
2. **map BAR1**：`PrepareHardware` 挑最大的 memory resource（RP1 BAR1 是大窗口），`MmMapIoSpaceEx`；BAR1 ≤64KB 表示 RP1 韌體沒起來，warn。
3. **動態 child list**：`WDFCHILDLIST` + `WdfChildListBeginScan/AddOrUpdateChildDescriptionAsPresent/EndScan`，依 `g_Rp1Periph[]`（UART/I2C/SPI/I2S/GPIO/ETH/USB/PWM/ADC/DMA…，offset=DT reg 低 32 位 -0x40000000，IRQ=RP1 內部 IC index）建子 PDO，HWID 如 `RP1\UART0`。

### 關鍵設計決策：子裝置資源用 **bus interface**，不偽造 CM_RESOURCE_LIST
- **問題**：RP1 內部周邊**共用同一個 BAR1**（只有 bus driver 該 map），且中斷由 **RP1 內部中斷控制器** demux（GpioClx 角色），不是離散 GSI → KMDF 沒有乾淨 API 把 CM 資源指派給 PDO（要降到 WDM `IRP_MN_QUERY_RESOURCES` 很髒）。
- **解法**：bus driver 在每個子 PDO 上 `WdfDeviceAddQueryInterface` 匯出 **`GUID_RP1BUS_INTERFACE_STANDARD`**（定義在 `rp1bus_if.h`，GUID `C7E9A1B2-4D3F-4A21-9B6E-2F1A7C0D5E88`）。
- **子 class driver 怎麼用**：在自己的 `EvtDevicePrepareHardware` 呼叫 `WdfFdoQueryForInterface(GUID_RP1BUS_INTERFACE_STANDARD)`，拿到 4 個回呼：
  - `GetRegisterBase()` → 該子裝置在 BAR1 裡**已映射的 kernel VA**（= 父 `BarBase` + 該子 `Offset`，即時計算）。
  - `GetRegisterSize()` / `GetInterruptIndex()`（RP1 內部 IRQ index）/ `GetPhysicalBase()`（DMA/debug 用）。
- **與既有模式一致**：同 WiFi `wifi/cyw43455/whd_port/sdbus_glue.c` 的「向 parent 查 hardware interface」做法。
- 介面 `Context` = 子 WDFDEVICE；回呼讀其 PDO context + 父 FDO 的即時 `BarBase`（存 `pdoCtx->Fdo` 指標，故 re-map 後仍正確）；`InterfaceReference/Dereference` 對子 device 做 `WdfObjectReference/Dereference`。

### 檔案
- `rp1bus_if.h`（新）：公開介面標頭，**隨子 driver 原始碼一起發**。
- `common.h`：`include rp1bus_if.h`；`RP1BUS_PDO_CONTEXT` 加 `Fdo` 指標。
- `driver.c`：`#define INITGUID`；擴充 `g_Rp1Periph[]`；4 個介面回呼；child 建立時 `WdfDeviceAddQueryInterface`。
- `rp1bus.inf`（新）：System class，match `PCI\VEN_1DE4&DEV_0001`，後裝。

### 還沒做（需實機）
- **RP1 內部中斷控制器**（`RP1_APBS_IRQ_BASE 0x108000`，61 條內部 IRQ ←→ 一條 PCIe MSI-X）的 demux：bus driver 要當 GpioClx 式 IC，把 PCIe MSI-X ISR fan-out 到各子裝置的 ISR。介面已先把 `Irq` index 傳下去；連接 ISR 的時序需實機。
- 真實 RP1 VEN/DEV ID 確認（INF 目前填 `1DE4/0001`，待 Pi5 `lspci` 核對）。
- 每個 child class driver（GPIO/I2C/… 的 `EvtDevicePrepareHardware`）改成查本介面取窗口，而非自己 map MMIO。
