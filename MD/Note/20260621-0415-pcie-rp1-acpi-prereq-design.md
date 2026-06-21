# 先決 track 設計：BCM2712 PCIe + RP1 列舉 + ACPI（Windows on ARM）

> 日期：2026-06-21 04:15
> 這是**最關鍵的先決 track**：沒有它，RP1 底下所有 I/O 驅動（音訊/UART/I2C/SPI/GPIO/Ethernet/USB…）在 Windows 上都看不到。

## 1. 問題本質
- Linux：RP1 掛在 PCIe (pcie2)，RP1 內部周邊**不是標準 PCI function**，而是 RP1 **BAR1** 內固定偏移的子區塊；Linux 用 **DT overlay**（`rp1-pci.dtso` → `of_platform_default_populate`）把這些子區塊變成 platform device。
- Windows：**無 DT**。`pci.sys`/`acpi.sys` 不會把動態配置的 PCI BAR 切片給 ACPI 子裝置，也不會自動為 DSDT 子節點生 PDO。

## 2. RP1 硬體事實（來自 `drivers/misc/rp1/rp1_pci.c` + `pci_ids.h`）
- **PCI ID**：`PCI\VEN_1DE4&DEV_0001`（vendor 0x1de4 = Raspberry Pi；device 0x0001 = RP1 C0）。
- **BAR**：用 **BAR1**（`pcim_iomap(pdev, 1, 0)`）；開機要求 `len(BAR1) > 0x10000`（RP1 韌體須先跑）。
- **中斷**：RP1 內部 **61 條 IRQ**（`RP1_INT_END=61`），各配一個 **MSI-X** vector（`pci_alloc_irq_vectors(.., 61, 61, PCI_IRQ_MSIX)`）。
- **內部中斷控制器 (APBS)**：BAR1 偏移 **`0x108000`**。MSI-X cfg 暫存器 `MSIX_CFG(x)=0x8+4*x`；寫 `+0x800`=SET、`+0xc00`=CLR；bit0=ENABLE、bit2=IACK、bit3=IACK_EN（level 用 IACK_EN）。

## 3. Windows 架構（建議：KMDF bus driver `rp1bus.sys` + GpioClx 中斷 mux）
| 角色 | 做法 |
|------|------|
| PCIe Root Complex | **不寫 driver**。UEFI 韌體做 PHY/link training/window 設定並出標準 **ECAM**（ACPI `MCFG` + `PNP0A08` host bridge），inbox `pci.sys` 列舉。執行期電源若需專有處理 → 寫 **PEP**（platform extension），非 PCI driver。 |
| RP1 端點 | **`rp1bus.sys`**（KMDF）綁 `VEN_1DE4&DEV_0001`，`EvtDevicePrepareHardware` 取得 BAR1 實體位址。 |
| 子裝置列舉 | `WdfChildListAddOrUpdateChildDescriptionAsPresent` 動態生 PDO；`EvtChildListCreateDevice` 把 BAR1 **切片**成各周邊的 `CmResourceTypeMemory`（用下方偏移表）給對應 PDO；`WdfPdoInitAssignAcpiName` 連到 DSDT 對應 ACPI 節點（取 `_DSD`/`GpioInt`）。 |
| 中斷 mux（難點） | `rp1bus.sys` 同時註冊成 **GpioClx 中斷控制器**，擁有 MSI-X、做 ISR；MSI-X 觸發時讀 RP1 內部 status，向 GpioClx 通報對應「pin」（= RP1 內部 IRQ 線）。子裝置在 ACPI 用 **`GpioInt`** 指向 RP1 節點 → Windows PNP 自動轉成標準 `CmResourceTypeInterrupt`，class driver 無感。 |
| 子裝置 class driver | UART→SerCx2(`ARMH0011` PL011)、I2C/SPI→SpbCx、GPIO→GpioClx、Ethernet→NDIS、USB→inbox xHCI/dwc3。 |

## 4. RP1 BAR1 子區塊偏移表（= DT 位址低位 − 0x40000000）+ 內部 IRQ
| 周邊 | 偏移 | size | RP1_INT |
|------|------|------|---------|
| SYSCFG/mailbox | 0x08000 | 0x4000 | (SYSCFG) |
| clocks | 0x18000 | 0x10038 | — |
| UART0..5 | 0x30000/0x34000/0x38000/0x3c000/0x40000/0x44000 | 0x100 | 25,42,43,44,45,46 |
| SPI8 | 0x4c000 | 0x130 | — |
| SPI0..5 | 0x50000..0x64000 (step 0x4000) | 0x130 | 19..24 |
| SPI6/7 | 0x68000/0x6c000 | 0x130 | — |
| I2C0..6 | 0x70000..0x88000 (step 0x4000) | 0x1000 | 7..13 |
| audio_out | 0x94000 | 0x4000 | 4 |
| PWM0/1 | 0x98000/0x9c000 | 0x100 | 5/41 |
| I2S0..2 | 0xa0000/0xa4000/0xa8000 | 0x1000 | 14/15/16 |
| GPIO (3 banks) | 0xd0000/0xe0000/0xf0000 | 0xc000 ea | IO_BANK0/1/2 = 0/1/2 |
| Ethernet (GEM) | 0x100000 | 0x4000 | 6 |
| **APBS IRQ ctrl** | **0x108000** | 0x4000 | — |
| CSI0/1 | 0x110000/0x128000 | — | MIPI0/1 = 47/48 |
| PIO | 0x178000 | 0x20 | — |
| SD/MMC0/1 | 0x180000/0x184000 | 0x100 | SDIO0/1 = 17/18 |
| DMA | 0x188000 | 0x1000 | 40 |
| USB0/1 (dwc3) | 0x200000/0x300000 | 0x100000 | 30/35 (+子) |
| DSI0/VEC/DPI | 0x118000/0x144000/0x148000 | 0x1000 | VIDEO_OUT=49 |

## 5. 執行流程（目標）
1. UEFI 出 ECAM → `pci.sys` 列舉，配 RP1 BAR1 實體位址。
2. Windows 載入 `rp1bus.sys`（match VEN_1DE4&DEV_0001）。
3. `rp1bus.sys` 註冊 GpioClx 中斷控制器 + 掛 MSI-X；`WdfChildListAddOrUpdateChildDescriptionAsPresent` 列舉 UART0…等。
4. 建 PDO 時切 BAR1 子窗 + `WdfPdoInitAssignAcpiName` 連 ACPI；PNP 看到 `ARMH0011` 等 HWID → 載入對應 class driver。
5. class driver 要資源時，PNP 把 MMIO 切片 + ACPI `GpioInt` 合成標準資源清單交給它。

## 6. 本 track 產出
- `windows_sources/pcie-rp1/acpi/rp1.asl` — DSDT 片段（PNP0A08 + RP1 + 子裝置 GpioInt），以 `...\Tools\10.0.26100.0\x64\asl.exe` 編譯。
- `windows_sources/pcie-rp1/rp1bus/` — KMDF bus driver 骨架（綁 PCI、map BAR1、子裝置偏移表、建 PDO）。
- 工具：`asl.exe`（WDK）；ASL 也可用 ACPICA `iasl`。

## 7. 仍需實機/韌體
- UEFI 對 BCM2712 PCIe 的 ECAM/MCFG 與 window 設定（`pcie-brcmstb` 的等效）。
- MSI-X→GIC 實際路由、RP1 韌體啟動（BAR1 才會 > 64KB）。
- 這些在 Windows-on-Pi5 韌體就緒前無法在實機驗證；本 track 提供可建置/可編譯的 driver + ASL 骨架與精確硬體對照，待韌體就緒即可接上。

## 8. 建置狀態 ✅ + KMDF know-how
- **`rp1.aml`**：`asl.exe` 編譯成功（PNP0A08 + RP1 + UART0/I2C0/I2S0 GpioInt）。
- **`rp1bus.sys`**：ARM64 KMDF bus driver **build 成功**（`windows_driver/pcie-rp1/rp1bus.sys`，13.8 KB，entry `FxDriverEntry`，import `WDFLDR.SYS` `WdfVersionBind`）。綁 `VEN_1DE4&DEV_0001`、map BAR1、`WdfFdoInitSetDefaultChildListConfig` + `WdfChildListAddOrUpdateChildDescriptionAsPresent` 列舉 10 個代表周邊為子 PDO（HWID `RP1\UART0` 等），每子記錄 BAR1 切片 `ChildPhys = BarPhys + Offset`。
- **KMDF 手動 cl/link 配方（首次驗證）**：
  - cl：include `wdf\kmdf\1.33` **置前**，再 `km\crt`/`km`/`shared`；加 `/DKMDF_VERSION_MAJOR=1 /DKMDF_VERSION_MINOR=33`。
  - link：entry **`/ENTRY:FxDriverEntry`**；libpath 加 `Lib\wdf\kmdf\arm64\1.33`；libs `wdfdriverentry.lib wdfldr.lib`（+ 既有 `libcntpr/BufferOverflowFastFailK/ntoskrnl/hal`）。
  - 坑：`#pragma alloc_text(INIT, DriverEntry)` 前要先 `DRIVER_INITIALIZE DriverEntry;` 宣告（C2157）。
- **下一步精修（仍可在無實機進行）**：
  1. 子 PDO 用 raw resource 報告 MMIO 切片（`WdfPdoInitAssignRawDevice` / `EvtDeviceResourceRequirementsQuery`）。
  2. `WdfPdoInitAssignAcpiName` 連 DSDT 子節點（取 GpioInt）。
  3. `rp1bus.sys` 註冊 **GpioClx 中斷控制器**（掛 MSI-X、讀 APBS@0x108000、demux 61 IRQ）。
  4. INF（含 WDF coinstaller 段）。
- **需實機/韌體**：MSI-X→GIC 實際路由、各子 class driver 在實機綁定。
