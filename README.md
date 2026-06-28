# AprWindowsARMDriverPI5

將 **Raspberry Pi 5**（Broadcom **BCM2712** SoC + **RP1** I/O 南橋）的 **Linux 驅動程式**移植到
**Windows on ARM (ARM64)** 平台的工作專案。

> Linux 與 Windows 驅動模型不同，本質是「**重寫**」而非重新編譯：可沿用暫存器定義、初始化序列、
> 通訊協定、演算法；必須重寫 OS 介接層（記憶體／中斷／DMA／電源／bus 列舉）。硬體描述由
> **Device Tree (DT) → ACPI (ASL)**。

---

## 專案目標與現況

- **命脈**：RP1 所有 I/O 掛在 BCM2712 PCIe 底下 → 需先打通 PCIe 列舉。PCIe RC 靠 UEFI ECAM
  (MCFG/PNP0A08) → inbox `pci.sys`；自製 `rp1bus.sys`（ARM64 KMDF bus driver）切 BAR1 給子裝置、
  做 MSI-X 中斷 demux；子裝置以 ACPI `GpioInt`（pin = RP1 內部 IRQ）列舉。
- **涵蓋裝置**：UART / I2C / SPI / I2S 音訊 / GPIO / PWM / ADC / PIO / DMA / Ethernet(GEM) /
  USB / SD-eMMC / RNG / mailbox / watchdog / clocks / RTC / 顯示(WDDM DOD) / GPU(V3D) /
  相機(CSI) / 藍牙 / WiFi 等。
- **韌體側（UEFI）✅ 可 build**：Pi5 跑 Windows 本就需刷社群 **worproject EDK2 UEFI**；我們在其 ACPI
  加入 RP1 全周邊 + V3D 的 Device 節點（`ACPI\RPIF000n`）+ 自訂 logo，**WSL 實測編出 `RPI_EFI.fd`**
  （[`uefi_fixed/`](uefi_fixed/)；配方 `MD/Skill/pi5-uefi-build.md`）。裝置描述進韌體、驅動本體用 DISM
  注入 Windows 映像（[`MD/Note/20260626-0130-...`](MD/Note/)）。**主線＝客製 UEFI ACPI（做法 A）**，
  `rp1bus.sys`（做法 B，純後裝 bus driver）降為備案。
- **GPU 3D 加速**：策略 **Vulkan-first**——寫一個 WDDM render KMD（`rp1-v3d`）+ port Mesa **v3dv**
  的 Windows winsys（`v3dv-wddm`），上層 **Zink(OpenGL)/DXVK·vkd3d(D3D)/clvk(OpenCL)** 全解鎖。
  KMD 可列舉空殼 + 4 個 sim 驗證純邏輯模組（PTE/CL-submit/MMU/ioctl 翻譯），已用 Pi5 實機校正 V3D 7.1.10.16。
- **接手/全專案總說明**：[`MD/HANDOFF.md`](MD/HANDOFF.md)（雙機模型、倉庫結構、關鍵決策、慣例、UEFI/GPU 線、16GB 問題、bring-up 第一步）。
- **單一進度真相**：[`MD/Note/RPi5-Porting-Status.md`](MD/Note/RPi5-Porting-Status.md)。
  硬體全清單對照：[`MD/Note/RPi5-Driver-Porting-Inventory.md`](MD/Note/RPi5-Driver-Porting-Inventory.md)。

### 狀態階梯
`⬜ 尚待 → 🟡 部分 → 🟢 骨架完成（可建置） → 🔵 邏輯完整（狀態機全移植＋x64 模擬驗證） →
✅ 完整完成（實機驗證／純軟體交付物） ｜ ➖ 免驅動（ACPI/inbox）`

---

## 雙機交接模型

無實機 + 無 Windows-on-Pi5 韌體時，硬體驅動無法「功能完整」（無法載入/驗證），故採雙機分工：

| 端 | 負責 | 天花板 |
|----|------|--------|
| **x64（開發機）** | cross-compile 產出、補完邏輯、寫 x64 模擬 harness 驗序列、寫 ACPI/INF、對 Linux 源碼比對 | 硬體驅動 → 🔵；純軟體交付物(ACPI/INF) → ✅ |
| **Pi5（實機）** | Windows-on-ARM 載入、KDNET 雙機 WinDbg、校時序/中斷/DMA | 🔵 → ✅ |

**工具鏈**：x64 主機**交叉編譯**至 ARM64，使用 WDK 10.0.26100 + VS2022 MSVC v143 ARM64 build tools。
各驅動以 `windows_sources/<類別>/<專案>/build.ps1`（直接呼叫 `cl`/`link`）建置。

---

## 兩條品質方法（本專案特色）

1. **x64 模擬驗證（sim pattern）**：每個 register-HAL 抽成乾淨 `void* Base` 邏輯 + regio shim，
   在 x64 user-mode 以 mock 暫存器 + 斷言跑過初始化/傳輸序列，**不需硬體即可驗證狀態機**。
   目前 18+ 引擎、數百條斷言全綠。
2. **Pi5 實測校正 + 源碼 B 類比對**：透過 SSH 連真 Pi5（跑 Linux）唯讀萃取 `lspci`/`iomem`/
   `device-tree`/`clk`/`devmem`，把「照源碼推測」的偏移/IRQ/時脈對**真矽晶片**校正（A 類）；
   再對每個 HAL 做「Linux 源碼 vs 我的移植」逐行比對，補回移植時漏抄的暫存器設定（B 類）。
   詳見 [`MD/Note/20260621-0720-pi5-linux-hardware-facts.md`](MD/Note/20260621-0720-pi5-linux-hardware-facts.md)
   與 [`MD/Note/20260621-0820-source-vs-port-bclass-gaps.md`](MD/Note/20260621-0820-source-vs-port-bclass-gaps.md)。

---

## 目錄結構

| 目錄 | 內容 |
|------|------|
| [`windows_sources/`](windows_sources/) | **Windows 端驅動原始碼**，依裝置類別分子目錄（HAL `.c/.h` + sim + `driver.c` + `build.ps1`；含 GPU `rp1-v3d` KMD 與 `v3dv-wddm` UMD winsys）。|
| [`windows_driver/`](windows_driver/) | **建置交付物**：各驅動 INF（已 infverif）。`.sys/.cat` 等二進位為 build 產出，未納版控。|
| [`uefi_fixed/`](uefi_fixed/) | **我們改過的 Pi5 UEFI source**（overlay 上游）：RP1 周邊 + V3D 的 ACPI 節點、自訂開機 logo。|
| `uefi_build/` | UEFI build 產出 `RPI_EFI.fd`（刷 SD 卡）。二進位不納版控、保留 README。|
| [`uefi_sources_backup/`](uefi_sources_backup/) | **可編譯的 Pi5 UEFI source 凍結備份**（30MB，端到端驗證；防上游 fork/鏡像消失）。|
| [`MD/`](MD/) | 分析筆記與文件（進度狀態、硬體清單、各階段 know-how、UEFI/GPU 策略）。|
| [`tools/`](tools/) | 輔助工具（如 Gemini 知識庫查詢腳本）。|
| `sources/` | （**未納版控**）Raspberry Pi Linux 核心原始碼 sparse checkout，移植參考。重建 → `MD/Skill/sources-rebuild.md`。|
| `uefi_sources/` `gpu_driver_sources/` | （**未納版控**）上游 UEFI(EDK2)／GPU(KMDOD/viogpu/Mesa) 參考源碼。重建 → `MD/Skill/*-rebuild.md`。|
| `temp/` `private/` | （**未納版控**）暫存／個人檔。|

各主要子目錄另有 `README.md` 說明。

---

## 建置（範例）

```powershell
# 在對應驅動目錄下（需 VS2022 + WDK 10.0.26100，x64 host → ARM64 cross）
cd windows_sources\i2c\rp1-dw-i2c
.\build.ps1                       # 產出 ARM64 .sys 到 windows_driver\i2c\

# x64 模擬驗證（不需硬體）
cl /DDWI2C_SIM /I.. sim\dwi2c_sim.c dw_i2c_hw.c /Fe:dwi2c_sim.exe && .\dwi2c_sim.exe
```

ACPI：`asl.exe windows_sources\pcie-rp1\acpi\rp1.asl` → `rp1.aml`。

---

## 注意

- 本專案為**移植開發中**：x64 端已把可做的推到 🔵（邏輯完整＋模擬過），純軟體交付物（ACPI/INF）達 ✅；
  **韌體側（含我們 ACPI 的 `RPI_EFI.fd`）已可 build**。最終功能驗證需 **Raspberry Pi 5 + Windows on ARM**
  實機（刷韌體、載入、中斷路由、DMA、GPU KDNET BSOD 除錯）。
- **韌體建置**需 WSL2（虛擬化開啟）+ EDK2；上游 UEFI/GPU 參考源碼未納版控，依各 `MD/Skill/*-rebuild.md` 重建。
- 暫存器定義/初始化序列移植自 [raspberrypi/linux](https://github.com/raspberrypi/linux)（GPL-2.0）；
  OS 介接層為本專案重寫。
