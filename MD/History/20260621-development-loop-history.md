# 開發歷程：自動移植 loop 紀錄（2026-06-21）

> 本檔收錄各階段「自動移植 loop」的完成紀錄與已解除的 cron job，純屬**歷程備查**。
> 即時規則/守則見 `../../CLAUDE.md`；單一進度真相見 `../Note/RPi5-Porting-Status.md`；
> 各階段 know-how 細節見對應 `../Note/年月日-時間-主題.md`。

---

## 1. 自動移植 loop 第一輪 — 音訊/顯示/GPU 骨架（job `28957e8c`，~04:05 解除）

- cron job `28957e8c`（每 5 分鐘，session-only），prompt「持續把 音效卡移植成 windows driver」。三類驅動皆達「無實機可建置骨架」天花板後 `CronDelete`。
- **產出**：音訊 `rp1i2s.sys`(WDM)+`rp1i2saud.sys`(PortCls/WaveRT)、顯示 `rp1vc4dod.sys`(WDDM DOD) 皆 build 成功；GPU `windows_sources/gpu/rp1-v3d/`(WDDM render scaffold) 可編譯。
- **原始路線**（依使用者指示）：
  1. 音效（無硬體天花板已達）✅：Phase 1 WDM 骨架 `rp1i2s.sys`；Phase 2 PortCls/WaveRT `rp1i2saud.sys`（import portcls.sys）。餘下 DataRangeIntersection/DMA/codec/真實 MMIO 需實機。
  2. GPU/顯示（WDDM）：Stage A = DOD（kmdod 式，先點亮 HDMI）→ Stage B = KMS modeset（vc4 HVS/PixelValve/HDMI，需實機）→ Stage C = V3D 加速。Stage A 已 build ✅（`rp1vc4dod.sys`，19 DDI stub，link `displib.lib`）；Stage C = V3D render 骨架（`DRIVER_INITIALIZATION_DATA`+`DxgkInitialize`）。
  3. 音訊+顯示皆達無實機可建置骨架天花板；GPU V3D 為最後骨架。
- 再進展需實機（Pi5 + Windows-on-Pi5 + RP1 PCIe 列舉 + ACPI + GPU UMD）。總結見 `../Note/20260621-0405-v3d-scaffold-and-loop-summary.md`、`../Note/20260621-0358-dod-built-v3d-next.md`。

## 2. 自動移植 loop 第二輪 — SpbCx 批次（job `be74294d`，~05:00 解除）

- 可乾淨移植的標準框架批次完成後 `CronDelete`。
- 新增 4 個 SpbCx 驅動：`rp1i2c`/`rp1spi`/`bcm2712i2c`/`bcm2712spi`；UART 改用 inbox `SerPl011`(ARMH0011)；GpioClx 因 ~18 callback 當時跳過。
- 剩「需特別處理」tier（GpioClx/sdport/AVStream/WDDM render/NDIS/USB/WiFi/韌體相依）。詳見 `../Note/20260621-0500-spbcx-batch-loop-summary.md`。

## 3. 先決 track：PCIe + RP1 列舉 + ACPI

- **已建置**：`windows_driver/pcie-rp1/rp1bus.sys`（ARM64 KMDF bus driver，綁 `VEN_1DE4&DEV_0001`、map BAR1、列舉 RP1 內部周邊為子 PDO）+ `windows_sources/pcie-rp1/acpi/rp1.aml`（asl.exe 編譯成功）。
- **架構**：PCIe RC 靠 UEFI ECAM(MCFG/PNP0A08)→inbox `pci.sys`；`rp1bus.sys` 切 BAR1 給子裝置 + 註冊 GpioClx 做 MSI-X 中斷 demux；子裝置 ACPI 用 GpioInt（pin = RP1 內部 IRQ）。
- **RP1 事實**：BAR1；中斷控制器 APBS@0x108000；61 條內部 IRQ↔MSI-X。完整偏移/IRQ 表 + 設計見 `../Note/20260621-0415-pcie-rp1-acpi-prereq-design.md`。
- **首次 KMDF 手動 build 配方**：entry `FxDriverEntry`、link `wdfdriverentry.lib`+`wdfldr.lib`、include `wdf\kmdf\1.33` 置前。

## 4. x64「邏輯完整」loop（job `5d19ab08`，~06:00 解除）

- **達成**：ACPI(`rp1.aml`)+INF(15/15)→✅；**9 個硬體引擎推到 🔵**（SDHCI/DW-I2C/DW-SPI/BCM-I2C/BCM-SPI/GEM-Ethernet/DW-I2S/GPIO中斷/BT-H4），共 **143 條 x64 模擬斷言**通過。
- **🔵 sim pattern**：HAL 抽暫存器狀態機 → reg-I/O shim（`<X>_SIM` mock / 否則 `READ/WRITE_REGISTER`）/「fake kernel」simshim → `sim/*_sim.c` mock+斷言 → 接 framework callback。模擬 exe 放 `temp/`。詳見 `../Note/20260621-0600-x64-logic-complete-loop-summary.md`。
- x64 天花板已達。剩：需實機（DMA/中斷路由/PCIe demux/DOD modeset）、大型框架（WiFi dot11/WDI、camera capture、PortCls DataRange、GPU UMD=Mesa 移植）、缺 SDK（BT bthx.h）。

## 5. bespoke KMDF 骨架 loop（job `e5d1fde0`，~07:00 解除）

- **+10 驅動**（#14–23）：PWM/RNG/mailbox/ADC/watchdog/DMA/clocks/PIO/BCM2712-GPIO 達 🔵（各帶 x64 sim），RTC 薄骨架 🟢（韌體型無 MMIO）。
- **bespoke KMDF 模板**：`common.h`+`driver.c`(EvtPrepareHardware map MMIO)+`<dev>_hw.h/.c`(clean `void*` HAL+regio shim)+`sim/`+`build.ps1`。詳見 `../Note/20260621-0700-bespoke-kmdf-batch-summary.md`。
- register-HAL 型全數耗盡。剩 LED/電源鍵屬消費者型（GPIO/HID）。當時統計：🟢5 ｜ 🔵18 ｜ 🟡2 ｜ ✅2 ｜ ➖5。

## 6. Pi5 實測校正 loop（job `d8511a69`，8 輪後解除）

- **8 輪迭代**依真矽晶片校正 **7 處錯誤規格**：USB IRQ(30/35→31/36)、RP1 I2C SCL(85→200MHz)、SDHCI base(50→200MHz)＋歸屬(RP1→BCM2712 SoC)、RP1 SPI BaudDiv(200MHz)、I2S CCR(漏設)、BCM SPI ClkDiv(750MHz)、BCM I2C DIV(72→100kHz/108MHz)。
- **補上規格**：ACPI 由 16 RP1 子裝置 +9 BCM2712 SoC 裝置（QWordMemory 真實位址+GIC GSIV：SD/eMMC/mailbox/rng/wdt/dma/gpio/rtc/BSC-I2C/SPI）；**25/25 全部 .sys 補齊 INF**。ASL 4529 B。sim 全綠。
- 剩餘屬 Pi5 實機階段（NDIS OID/SDHCI bus ops/中斷路由/DMA/載入）。詳見 `../Note/20260621-0720-pi5-linux-hardware-facts.md`。

## 7. 原始碼 B 類缺漏修正 loop（job `2dc34690`，解除）

- **規格缺漏 A/B 兩類原則**（重要，寫在 `../Note/20260621-0720-pi5-linux-hardware-facts.md`）：A 類=runtime 才解析（時脈/位址/GIC/硬體能力），源碼只有公式、值要量實機；B 類=源碼有寫但移植時漏抄，重讀源碼即可補。
- **7 個並行 agent** 對所有 register-HAL 驅動做「Linux 源碼 vs 移植」逐項比對，找出 B 類缺漏；**14 個驅動全數補正、各帶 x64 sim 驗證**（GEM/BCM-I2C/PWM/clk/BCM-SPI/ADC/RNG/I2S/DW-I2C/DW-SPI/GPIO/SDHCI/DMA40）。
- 代表性修正：PWM SET_UPDATE（否則設定不生效）、GEM MDC 分頻（否則 MDIO 爆表）、SDHCI TRANSFER_MODE/TIMEOUT/INT 錯誤位、DMA40 整組 40-bit register map、GPIO PADS pull、DW-SPI reset-chip+DFS32 自適應偵測、clk PLL_PRIM RMW、ADC RWTYPE alias、RNG BCM2711 暖機。詳見 `../Note/20260621-0820-source-vs-port-bclass-gaps.md`。
- 剩餘只有需實機跑才驗的時序/中斷/DMA 行為（🔵→✅，Pi5 階段）。

---

## cron job 對照（皆已 `CronDelete` 解除）

| job | 階段 | 結果 |
|-----|------|------|
| `28957e8c` | 音訊/顯示/GPU 骨架 | 三類達無實機骨架天花板 |
| `be74294d` | SpbCx 批次 | +4 SpbCx 驅動 |
| `5d19ab08` | x64 邏輯完整 | 9 引擎 🔵 + ACPI/INF ✅ |
| `e5d1fde0` | bespoke KMDF | +10 驅動 |
| `d8511a69` | Pi5 實測校正 | 7 規格修正 + ACPI/INF 擴充 |
| `2dc34690` | 源碼 B 類缺漏 | 14 驅動補正 |
