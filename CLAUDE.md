# 專案守則 (Project Charter)

## 溝通語言
- **一律使用繁體中文與使用者對談。** 技術名詞（API、framework、暫存器、compatible 字串等）可保留英文。

## 專案目標
將 **Raspberry Pi 5**（BCM2712 SoC + RP1 I/O 南橋）的 **Linux 驅動程式**移植到 **Windows on ARM (ARM64)** 平台。

## 重要路徑
- `sources/` — Raspberry Pi Linux 核心原始碼（raspberrypi/linux）。本次為 sparse checkout，含：`drivers/`、`arch/arm64/boot/dts/broadcom/`（device tree）、`arch/arm/boot/dts/overlays/`、`include/`、`Documentation/devicetree/bindings/`。
- `MD/Note/` — 分析筆記與文件（新筆記檔名格式：`年月日-時間-主題.md`，例 `20260621-0303-rp1-i2s-port.md`）。
  - `RPi5-Driver-Porting-Inventory.md` — 硬體裝置清單與 Windows 驅動移植對照表。
- `windows_sources/` — Windows 端驅動**原始碼**，依裝置類別分子目錄（如 `audio/`、`net/`、`usb/`、`gpio/`…，`_common/` 放共用程式碼）。
- `windows_driver/` — **build 產出**（.sys/.inf/.cat），同樣依類別分目錄。
- `temp/` — **暫存性質**的下載/快取一律放這裡，勿散落專案根目錄。

## 移植原則
1. Linux 與 Windows 驅動模型不同，本質為「**重寫**」而非「重新編譯」。
2. 可沿用：暫存器定義、初始化序列、通訊協定、演算法；必須重寫：OS 介接層（記憶體、中斷、DMA、電源、bus 列舉）。
3. 硬體描述需由 **Device Tree (DT) → ACPI (ASL)**。
4. Windows 框架對應：KMDF/UMDF、NDIS（網路）、SerCx2（UART）、SpbCx（I2C/SPI）、GpioClx（GPIO）、WDDM（顯示/GPU）、AVStream（相機）、PortCls（音訊）。
5. **PCIe 是命脈**：RP1 所有 I/O 掛在 BCM2712 PCIe 底下，需先打通 PCIe 列舉。

## 注意事項
- 本工作目錄目前**非 git repository**（`sources/` 自身有獨立的 .git）。
- `sources/` 為 blobless + sparse 部分 clone；存取 cone 以外的檔案會觸發網路抓取。
- 音訊驅動 `sound/soc/`（含 `dwc/` DesignWare I2S）**已補抓**。
- **暫存/下載**檔案一律放 `temp/`；**建置產出**放 `windows_driver/`。
- **建置工具鏈（已安裝）**：x64 主機**交叉編譯**至 ARM64，使用 WDK 10.0.26100 + VS2022「MSVC v143 ARM64 build tools」。
- winget 版 WDK **不含** VS 的 MSBuild Driver 整合（無 `WDK.vsix`）；故各驅動以 `windows_sources\<類別>\<專案>\build.ps1`（**直接呼叫 cl/link**）建置。include 順序須 `km\crt`→`km`→`shared`（勿加 VC user-mode include）。詳見 `MD/Note/20260621-0331-rp1-i2s-audio-port.md`。
- 遇技術/規格問題可用 `tools/knowledgebase/gemini_query.py`（金鑰 `C:\key\config.json`）諮詢 Gemini 參考。

## 進度追蹤規範（重要）
- **單一進度真相來源：`MD/Note/RPi5-Porting-Status.md`**（全 Pi5 驅動移植狀態：⬜尚待／🟡部分／🟢骨架／🔵邏輯完整／✅完整／➖免驅動）。
- **每完成一個階段性產出就立即更新此檔**：driver build 成功、scaffold 編譯通過、ASL 編譯、子系統完成等，皆要更新對應列的「狀態／產出路徑／餘下項目」，必要時調整頁首統計數字。
- 同時依既有慣例新增 `MD/Note/年月日-時間-主題.md` 記錄該階段 know-how。
- 硬體全清單對照：`MD/Note/RPi5-Driver-Porting-Inventory.md`。

### 雙機交接模型（2026-06-21 使用者定案）
- **x64 端（本機）負責**：cross-compile 產出、補齊邏輯、寫模擬 harness 驗證序列、寫 ACPI/INF。硬體驅動推到 **🔵 邏輯完整**（邏輯全移植＋x64 模擬過）天花板；純軟體交付物（ACPI/INF/inbox）可達 **✅ 完整完成**。
- **Pi5 端（日後實機）負責**：Windows-on-ARM 載入、KDNET 雙機 WinDbg、校時序/中斷/DMA → 把 🔵 推到 ✅。
- **狀態階梯**：⬜→🟡→🟢骨架→🔵邏輯完整→✅完整（硬體類需 Pi5）。定義詳見 `RPi5-Porting-Status.md` 頁首「狀態定義」＋「交接盤點」。
- **x64 端優先序**：① ACPI 補完＋各驅動 INF（→✅）→ ② 核心 HAL 推到 🔵（先 SDHCI/Ethernet/I2C）＋模擬 harness。

### x64「邏輯完整」loop 已完成並解除（2026-06-21 06:00，job `5d19ab08`）
- **達成**：ACPI(`rp1.aml`)+INF(15/15)→✅；**9 個硬體引擎推到 🔵**（SDHCI/DW-I2C/DW-SPI/BCM-I2C/BCM-SPI/GEM-Ethernet/DW-I2S/GPIO中斷/BT-H4），共 **143 條 x64 模擬斷言**通過。
- **🔵 sim pattern**：HAL 抽暫存器狀態機 → reg-I/O shim（`<X>_SIM` mock / 否則 `READ/WRITE_REGISTER`）/「fake kernel」simshim → `sim/*_sim.c` mock+斷言 → 接 framework callback。模擬 exe 放 `temp/`。詳見 `MD/Note/20260621-0600-x64-logic-complete-loop-summary.md`。
- **x64 天花板已達**。剩：需實機（DMA/中斷路由/PCIe demux/DOD modeset）、大型框架（WiFi dot11/WDI、camera capture、PortCls DataRange、GPU UMD=Mesa 移植）、缺 SDK（BT bthx.h）。

### bespoke KMDF 骨架 loop 已完成並解除（2026-06-21 07:00，job `e5d1fde0`）
- **+10 驅動**（#14–23）：PWM/RNG/mailbox/ADC/watchdog/DMA/clocks/PIO/BCM2712-GPIO 達 🔵（各帶 x64 sim），RTC 薄骨架 🟢（韌體型無 MMIO）。**bespoke KMDF 模板**：`common.h`+`driver.c`(EvtPrepareHardware map MMIO)+`<dev>_hw.h/.c`(clean `void*` HAL+regio shim)+`sim/`+`build.ps1`。詳見 `MD/Note/20260621-0700-bespoke-kmdf-batch-summary.md`。
- **register-HAL 型全數耗盡**。剩 LED/電源鍵屬消費者型（GPIO/HID，非獨立 register 驅動）。全專案：🟢5 ｜ 🔵18 ｜ 🟡2 ｜ ✅2 ｜ ➖5。

## 自動移植 loop — 已完成並解除（2026-06-21 04:05）
- **Loop（job `28957e8c`）已 `CronDelete` 解除**：三類驅動皆達「無實機可建置骨架」天花板。
- **產出**：音訊 `rp1i2s.sys`(WDM)+`rp1i2saud.sys`(PortCls/WaveRT)、顯示 `rp1vc4dod.sys`(WDDM DOD) 皆 build 成功；GPU `windows_sources/gpu/rp1-v3d/`(WDDM render scaffold) 可編譯。
- **再進展需實機**（Pi5 + Windows-on-Pi5 + RP1 PCIe 列舉 + ACPI + GPU UMD）。總結見 `MD/Note/20260621-0405-v3d-scaffold-and-loop-summary.md`。
- 要恢復：重新 `/loop` 或指定 track；最關鍵先決 track = BCM2712 PCIe RC + RP1 列舉 + ACPI。

## 先決 track：PCIe + RP1 列舉 + ACPI（進行中，2026-06-21）
- **已建置**：`windows_driver/pcie-rp1/rp1bus.sys`（ARM64 KMDF bus driver，綁 `VEN_1DE4&DEV_0001`、map BAR1、列舉 RP1 內部周邊為子 PDO）+ `windows_sources/pcie-rp1/acpi/rp1.aml`（asl.exe 編譯成功）。
- **架構**：PCIe RC 靠 UEFI ECAM(MCFG/PNP0A08)→inbox pci.sys；`rp1bus.sys` 切 BAR1 給子裝置 + 註冊 GpioClx 做 MSI-X 中斷 demux；子裝置 ACPI 用 GpioInt（pin = RP1 內部 IRQ）。
- **RP1 事實**：BAR1；中斷控制器 APBS@0x108000；61 條內部 IRQ↔MSI-X。完整偏移/IRQ 表 + 下一步見 `MD/Note/20260621-0415-pcie-rp1-acpi-prereq-design.md`。
- **首次 KMDF 手動 build 配方**：entry `FxDriverEntry`、link `wdfdriverentry.lib`+`wdfldr.lib`、include `wdf\kmdf\1.33` 置前。

## 自動移植 loop 第二輪 — 已完成並解除（2026-06-21 05:00）
- **Loop（job `be74294d`）已 `CronDelete`**：可乾淨移植的標準框架批次完成。
- 本輪新增 4 個 SpbCx 驅動：`rp1i2c`/`rp1spi`/`bcm2712i2c`/`bcm2712spi`；UART 改用 inbox `SerPl011`(ARMH0011)；GpioClx 因 ~18 callback 跳過。
- 剩下屬「需特別處理」tier（GpioClx/sdport/AVStream/WDDM render/NDIS/USB/WiFi/韌體相依），逐項待討論。詳見 `MD/Note/20260621-0500-spbcx-batch-loop-summary.md` 與進度清單 `RPi5-Porting-Status.md`。

### （歷史）loop 原始設定
- cron job `28957e8c`（每 5 分鐘，session-only），prompt「持續把 音效卡移植成windows driver」。
- **路線（依使用者 2026-06-21 指示）**：
  1. **音效（無硬體天花板已達）✅**：Phase 1 WDM 骨架 `rp1i2s.sys`；Phase 2 PortCls/WaveRT `rp1i2saud.sys`（**已 build**，import portcls.sys）。餘下 DataRangeIntersection/DMA/codec/真實 MMIO 需實機。
  2. **目前階段：GPU/顯示（WDDM）**。路線：Stage A = Display-Only Driver（DOD，kmdod 式，先點亮 HDMI）→ Stage B = KMS modeset（vc4 HVS/PixelValve/HDMI）→ Stage C = V3D 加速。**Stage A（DOD）已 build ✅**：`windows_driver/display/rp1vc4dod.sys`（19 DDI stub，link `displib.lib`）。Stage B（vc4 modeset 點亮畫面）需實機。**目前：Stage C = GPU V3D WDDM render 骨架**，專案 `windows_sources/gpu/rp1-v3d/`（render 用 `DRIVER_INITIALIZATION_DATA`+`DxgkInitialize`，非 DOD 的 KMDDOD）。詳見 `MD/Note/20260621-0358-dod-built-v3d-next.md`。
  3. **整體**：音訊(`rp1i2s.sys`,`rp1i2saud.sys`)+顯示(`rp1vc4dod.sys`)皆達無實機可建置骨架天花板；GPU V3D 為最後骨架，完成即抵達無實機無法再進展點 → 解除 loop。
- **過程持續把 know-how 寫成 `MD/Note/年月日-時間-主題.md`**。
- **解除 loop 條件**：使用者另行指示，或抵達「無實機/無法再進展」之點（屆時 `CronDelete 28957e8c` 並 PushNotification 告知）。

## Pi5 實機（Linux）SSH — 硬體真相校正（2026-06-21）
- **有一台真 Pi5**（BCM2712，跑 **Debian 13 aarch64**，非 Windows）：`ssh -i ~/.ssh/aprvisual_pi -o BatchMode=yes pi@192.168.0.66 '<cmd>'`（git-bash；IP 非固定，重開機可能變；`sudo -n` 免密碼）。
- **用途＝硬體真相**（唯讀：lspci/iomem/device-tree/clk/devmem/ftrace）。**不能跑/驗證 Windows .sys**（最終 🔵→✅ 仍需 Pi5 + Win11-ARM）。
- **已校正/驗證**（詳見 `MD/Note/20260621-0720-pi5-linux-hardware-facts.md`）：RP1 PCIe `1de4:0001` @ BAR1 `0x1f00000000`(4M)、MSI-X 61、周邊偏移全中（mailbox/clocks/pwm/adc@c8000/gpio×3/eth/pio/dma/usb）、IRQ eth=6·dma=40·mailbox=58·usb=31/36(修正)、**clk_sys=200MHz**(I2C/SPI/SDIO 分頻基準)、devmem 讀 PLL_SYS CS=0x80000001 驗 `PLL_CS_LOCK=BIT(31)`。
- **注意**：此為使用者 benchmark 機，**全程唯讀**，勿 devmem 寫入/勿讀未上電周邊（可能 bus-error）。

### Pi5 實測校正 loop 已完成並解除（2026-06-21，job `d8511a69`）
- **8 輪迭代**依真矽晶片校正 **7 處錯誤規格**：USB IRQ(30/35→31/36)、RP1 I2C SCL(85→200MHz)、SDHCI base(50→200MHz)＋歸屬(RP1→BCM2712 SoC)、RP1 SPI BaudDiv(200MHz)、I2S CCR(漏設)、BCM SPI ClkDiv(750MHz)、BCM I2C DIV(72→100kHz/108MHz)。
- **補上規格**：ACPI 由 16 RP1 子裝置 +9 BCM2712 SoC 裝置（QWordMemory 真實位址+GIC GSIV：SD/eMMC/mailbox/rng/wdt/dma/gpio/rtc/BSC-I2C/SPI）；**25/25 全部 .sys 補齊 INF**。ASL 4529 B。sim 全綠。
- 剩餘屬 Pi5 實機階段（NDIS OID/SDHCI bus ops/中斷路由/DMA/載入）。詳見 `MD/Note/20260621-0720-pi5-linux-hardware-facts.md`。

### 原始碼 B 類缺漏修正 loop 已完成並解除（2026-06-21，job `2dc34690`）
- **規格缺漏 A/B 兩類原則**（重要，寫在 `20260621-0720-...md`）：A 類=runtime 才解析（時脈/位址/GIC/硬體能力），源碼只有公式、值要量實機；B 類=源碼有寫但移植時漏抄，重讀源碼即可補。
- **7 個並行 agent** 對所有 register-HAL 驅動做「Linux 源碼 vs 移植」逐項比對，找出 B 類缺漏；**14 個驅動全數補正、各帶 x64 sim 驗證**（GEM/BCM-I2C/PWM/clk/BCM-SPI/ADC/RNG/I2S/DW-I2C/DW-SPI/GPIO/SDHCI/DMA40）。
- 代表性修正：PWM SET_UPDATE（否則設定不生效）、GEM MDC 分頻（否則 MDIO 爆表）、SDHCI TRANSFER_MODE/TIMEOUT/INT 錯誤位、DMA40 整組 40-bit register map、GPIO PADS pull、DW-SPI reset-chip+DFS32 自適應偵測、clk PLL_PRIM RMW、ADC RWTYPE alias、RNG BCM2711 暖機。詳見 `MD/Note/20260621-0820-source-vs-port-bclass-gaps.md`。
- 剩餘只有需實機跑才驗的時序/中斷/DMA 行為（🔵→✅，Pi5 階段）。
