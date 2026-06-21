# Pi5 (Linux) 實測硬體事實 — 校正/驗證驅動硬體層

> 2026-06-21　｜ 經 SSH 進 Pi5（Debian 13 aarch64, kernel 6.18.34, BCM2712）唯讀萃取。
> 用途：把先前「照 Linux 源碼推測」的偏移/IRQ/時脈/常數，對**真矽晶片**校正。連線備忘見使用者提供之 Pi5 SSH 筆記（`pi@192.168.0.66`，金鑰 `~/.ssh/aprvisual_pi`）。

## ✅ 確認正確（與我設計表一致）
### RP1 over PCIe
- **PCI ID `1de4:0001`** ✓（驅動綁 `VEN_1DE4&DEV_0001`）；拓樸 = BCM2712 PCIe bridge(`0002:00:00.0`) → RP1(`0002:01:00.0`)，PCIe segment **0002**。
- **BAR1 = 實體 `0x1f00000000`，size 4M** ✓（驅動 map BAR1）。BAR0=`0x1f00410000`(16K, MSI-X table)、BAR2=`0x1f00400000`(64K, SRAM)。
- **MSI-X Count = 61** ✓（精準對上「61 內部 IRQ」）。MSI-X table 在 **BAR0 off 0**，PBA BAR0 off 0x2000。RP1 PCIe pin A → host IRQ 44。

### RP1 BAR1 周邊偏移（實測 iomem，全中）
| 周邊 | 實體位址 | 偏移 |
|------|---------|------|
| mailbox | 1f00008000 | **0x08000** ✓ |
| clocks | 1f00018000 (size 0x10038) | **0x18000** ✓ |
| pwm | 1f0009c000 | **0x9c000** ✓ |
| **adc** | 1f000c8000 | **0xc8000**（新確認）|
| gpio bank0/1/2 | 1f000d0000/e0000/f0000 | **0xd0000/e0000/f0000** ✓ |
| ethernet(GEM) | 1f00100000 | **0x100000** ✓ |
| pio | 1f00178000 | **0x178000** ✓ |
| dma | 1f00188000 | **0x188000** ✓ |
| usb0/1 | 1f00200000/300000 | **0x200000/300000** ✓ |

### RP1 內部 IRQ（rp1_irq_chip）
- **eth0 = 6** ✓　**dma = 40** ✓（精準對上）。
- **mailbox = 58**（新）。

### devmem 驗證 HAL 常數
- **PLL_SYS CS @`0x1f00020000`（clocks 區 +0x8000）= `0x80000001`** → bit31 = **PLL_CS_LOCK** ✓（對上 clk HAL `PLL_CS_LOCK=BIT(31)`，且 pll_sys 已 lock/啟用）。

## ✏️ 校正（與我先前推測不同）
- **USB IRQ**：實測 usb1=**31**、usb3=**36**（我表寫 30/35）→ dwc3 host 用 31/36。
- ADC 偏移先前未定，實測 **0xc8000**。

## 🕐 真實時脈（先前標「需實機校正」→ 現確認）
| 時脈 | 頻率 | 用途 |
|------|------|------|
| xosc | 50 MHz | 晶振 |
| **clk_sys** | **200 MHz** | **RP1 I2C/SPI/SDIO 來源**（DW I2C SCL / DW SPI BAUDR 分頻基準）|
| pll_sys_core / sec | 1 GHz / 125 MHz | |
| clk_uart | 50 MHz | RP1 UART |
| pll_audio / core | 61.44 MHz / 1.536 GHz | RP1 I2S 音訊 |
| clk_sdio_alt_src | 200 MHz | RP1 SD/MMC |
| (BCM2712) uart-clock | 44.2368 MHz | BCM debug UART(107d001000) |
| (BCM2712) vpu-clock | 750 MHz | BCM SPI(107d004000) |

→ **DW I2C SCL HCNT/LCNT、DW SPI BAUDR、SDHCI clock divider 現可用 200 MHz 實算**（取代 HAL 內的暫填值）。

## ⚠️ 兩套裝置別搞混
- **RP1**（`1f00…`，PCIe BAR1，中斷走 `rp1_irq_chip`→MSI-X）：I2C/SPI/UART/I2S/GPIO/ADC/PWM/PIO/DMA/ETH/USB。
- **BCM2712 SoC**（`107d…`，直接掛 GIC）：debug UART(107d001000)、SPI(107d004000, GIC150)、I2C(107d508200/280)、GPIO(107d508500, pwr_button pin20)、SD(mmc0/1 = GIC305/306)。
  → 我的 `bcm2712i2c`/`bcm2712spi`/`bcm2712gpio` 對應這套（`107d…`，非 RP1）。

## 🧭 Pi5 端對照原則：「規格缺漏」的 A/B 兩類（重要）
> 有原始 Linux 源碼≠所有規格都能從源碼確認。缺漏分兩類，Pi5 端校驗時依此判斷該「查源碼」還是「量實機」：

### A 類 — 本質 runtime 解析，**源碼只有公式、值要問活機器**
源碼寫的是演算法，輸入在 boot 時才從活系統取得：
- **時脈頻率**：源碼 `clk_get_rate()` 取值再分頻；實際 Hz 由韌體設 PLL → 量 `clk_summary`。例：GEM `gem_mdc_clk_div()` 只說「pclk≤240MHz→DIV96」，要量到 pclk=200MHz 才知選 DIV96。
- **絕對實體位址**：源碼/DT 只有偏移；BAR/實體位址 PCIe 列舉時指派 → 讀 `/proc/iomem`。
- **中斷 GSIV**：源碼/DT 有中斷樹拓樸；最終 GIC 號 boot 時算出 → 讀 `/proc/interrupts`（GIC SPI = DT號+32；經 L2-intc 串接者取 L2 的 GIC 線）。
- **實際生效的配置**：源碼支援多板多 variant；這塊板用哪顆（SD=BCM2712、USB IRQ=31）→ 看 DT overlay + 實機。
- **硬體能力暫存器**：FIFO 深度/DBW 等 `readl(COMP_PARAM/DESIGNCFG)` → devmem 讀。

### B 類 — **源碼其實有，是移植時簡化/漏抄**（重讀源碼即可補，實機只作確認）
- 例：GEM `GemConfigure` 原本只 `SPD|FD`，漏了 `macb_init_hw` 必設的 **MDC 分頻(CLK)＋DRFCS**（後果：MDC=pclk/8=25MHz 爆 2.5MHz 上限→PHY 失敗）。已補。
- 例：I2C SCL 分頻、I2S CCR、SPI BAUDR——源碼都有，原本留 placeholder/TODO。
- **判別法**：若源碼該函式有寫該暫存器但我 HAL 沒有 → B 類，補。若源碼是 `xxx_get_rate()`/`platform_get_resource()`/`readl(cap)` 取值 → A 類，量實機。

### GEM 範例（A+B 同時）
源碼有 `gem_mdc_clk_div` 公式(B 的依據)＋我漏抄(B)＋實機定 pclk=200MHz(A)＋devmem 讀 `NCFGR=0x01560048` 三方對上 → 才確定 CLK=DIV96。

## 仍需 Windows-on-ARM 實機（SSH-Linux 跨不過）
載入/執行/除錯 `.sys`、Windows 框架（SpbCx/GpioClx/NDIS/SdPort）整合、WoA 開機+ACPI 列舉+PnP 綁定、KDNET WinDbg、MSI-X→GIC 在 Windows 的路由。

## 🔧 已套用之修正（loop `d8511a69`，依實測校正程式碼）
- **[#1] ACPI `rp1.asl`**：USB0/1 GpioInt **30/35 → 31/36**（runtime 確認）。asl.exe 重編 OK（3393 B）。
- **[#2] `dw_i2c_hw.c` SCL counts**：原值按 ~80–87 MHz 估（SS 0x190/0x1d6、FS 0x3c/0x82），改按實測 **clk_sys=200 MHz** 用 DesignWare 公式重算（SS **0x359/0x3E7**、FS **0xB1/0x13F**）。x64 sim 17/17 通過。

## 💾 SD/eMMC 真相（重大歸屬修正）
- Pi5 兩顆 MMC **都在 BCM2712**（**非 RP1**；RP1 SDIO 在此 config 未啟用）：
  | host | 實體位址 | DT SPI | GSIV(ACPI Interrupt) | 角色 |
  |------|---------|--------|------|------|
  | mmc0 | `0x1000FFF000`(+cfg `0x400`) | 273(0x111) | **305** | eMMC（soc@107c）|
  | mmc1 | `0x1001100000`(+cfg `0x400`) | 274(0x112) | **306** | SD 卡槽（axi）|
- **base clock = emmc2-clock = 200 MHz**（兩顆共用）。布局 = host(@+0x000, 0x260B) + cfg(@+0x400, 0x200B)。
- GSIV = DT SPI + 32（GIC SPI 慣例）。

## 📋 待查/待修 backlog（後續 loop 迭代）
- **[#3 ✅完成]** ~~SPI BAUDR~~：已補 `DwSpiHwBaudDiv(SclkHz)`（fssi=200 MHz，SCKDV 偶數），SPI sim 21/21。
- **[#4 ✅完成]** ~~SDHCI base clock + 歸屬~~：`BaseClockFrequencyKhz` 50000→**200000**；ACPI 把 SD 從 RP1 子裝置改為 `\_SB.SDC0/SDC1` BCM2712 SoC（QWordMemory 真實位址 + GIC 305/306），ASL 重編 3427 B。
- **[#5 ✅完成]** ~~I2S CCR~~：`Rp1I2sHwSetResolution` 漏設 CCR（Linux 同 switch 設 ccr 16→0x00/24→0x08/32→0x10）且 Config 沒寫 CCR → 補上 `Hw->Ccr` + `Rp1I2sWrite(CCR)`。sim 24/24（+4 CCR 斷言）。
- **[#6 ✅完成]** ~~ACPI ADC 缺~~：補 `\_SB.PCI0.RP1.ADC0`（_HID **RPI5000B**，輪詢免 IRQ，reg @BAR1+0xC8000）；新建 `windows_driver/adc/rp1adc.inf`（綁 ACPI\RPI5000B，KMDF 1.33，infverif 與已驗證 INF 同）。ASL 3478 B。
- **bespoke KMDF 缺 INF**（盤點 9 個 .sys 無 INF）：
  - **[RP1 系 ✅完成]** rp1adc(RPI5000B)/rp1pwm(RPI50005)/rp1pio(RPI5000C)/rp1clk(RPI5000D) → INF 建妥 + ACPI device 補齊（PWM 既有；PIO/CLK/ADC 新增），ASL 3580 B，infverif=valid。
  - **[BCM2712 系 ✅大半完成]** 實測 SoC 位址+GSIV：mailbox `0x107C013880`/GIC65、rng `0x107D208000`(輪詢)、wdt `0x107D200000`(無IRQ)、dma `0x1000010000`/GIC119-122。已加 ACPI device（MBOX/RNG0/WDT0/DMAC，HID BCM2EB0-3，QWordMemory+Interrupt）+ INF（bcmmbox/bcmrng/bcmwdt/bcmdma）。ASL 4003 B，infverif valid。
  - **[✅完成]** bcm2712gpio（BCM2EB5，2 bank `0x107D508500`+`0x107D517C00`，串接 gpio.irq0→l2-intc@7d508400→GIC244→**GSIV 276**）+ rpirtc（BCM2EB6，韌體無 MMIO，走 mailbox #16）。
  - **🎉 全部 25 個 .sys 現在都有對應 INF（0 缺漏）**＋ACPI device。ASL 4208 B。

## 📋 backlog（後續迭代）
- **[#7 ✅完成]** BCM SPI 分頻：補 `BcmSpiHwClkDiv(SclkHz)`（core=vpu **750 MHz** 實測，CDIV 偶數、0=65536）。BCM SPI sim 17/17。
- **[#8 ✅完成]** BCM I2C/SPI ACPI device：補 `BSC0/BSC1`(BCM2EA1, `0x107D508200/280`, L2@7d508380→GIC242→GSIV **274**) + `SPI0`(BCM2EA2, `0x107D004000`, GIC118→GSIV **150**)，對齊既有 INF HID。ASL 4529 B。
- **[#9 ✅完成]** BCM I2C core clock：Pi5 確認 = **108 MHz**（DT `clk-108M` / `108MHz-clock`；i2c `clock-frequency=0x17cdc`=97500 為 rpi 100kHz 慣例值，與 108MHz 自洽）。原預設 DIV=0x5dc 假設 150MHz → 真實 108MHz 下只有 **72 kHz**（bug）。修正預設→**0x438**(1080=100kHz) + 補 `BcmI2cHwDivForHz`(core=108MHz)。BCM I2C sim 17/17。

## ✅ Loop d8511a69 校正彙總（依 Pi5 實測修正的規格）
1. ACPI USB IRQ 30/35→31/36　2. RP1 I2C SCL counts(85→200MHz)　3. SDHCI base 50→200MHz＋歸屬 RP1→BCM2712　4. RP1 SPI BaudDiv(200MHz)　5. I2S CCR(漏設)　6. BCM SPI ClkDiv(750MHz)　7. BCM I2C DIV(72→100kHz,108MHz)
- **補上的規格**：ADC ACPI device＋9 個 bespoke KMDF INF（25/25 全有 INF）＋9 個 BCM2712 SoC ACPI device＋BCM I2C/SPI ACPI device。
- **sim 全綠**：RP1 I2C 17、RP1 SPI 21、I2S 24、BCM SPI 17、BCM I2C 17。ASL 最終 4529 B。
  - 註：ACPI DMA0(RPI5000A) 是 **RP1 dw-axi-dmac**(IRQ40，實機確認 `dw_axi_dmac_platform`)，與 `bcmdma.sys`(BCM2712 bcm2835-dma) 是**兩顆不同 DMA**；RP1 dw-axi-dmac 尚無 .sys（forward-declared）。
- **I2S sample-rate 分頻**：在 RP1 clock block（audio PLL 61.44 MHz÷20=3.072 MHz=48k×64），非 DW I2S HAL 內 → 屬 clk-rp1 驅動（已 🔵），非 I2S HAL bug。
- **BCM2712 I2C/SPI/UART 時脈**：BCM SPI=vpu 750 MHz、BCM UART=44.2368 MHz（與 RP1 那套不同，勿混用）。
- **BCM2712 I2C(107d508200/280)**：GIC GSIV？（DT interrupt-controller@7d508380 1/2 → 待換算）。

## 可再萃取（下次 SSH，唯讀）
- `dtc`/DT 拿 I2C/SPI/UART/I2S/SD 完整偏移 + 各自 RP1 IRQ（補完 IRQ 表）。
- devmem 讀各周邊身分暫存器驗 HAL 常數：DW I2C `COMP_TYPE`(0x44570140)、SDHCI caps、GEM module-id（**注意：未上電/未啟用的周邊勿讀，可能 bus-error**）。
- ftrace 追 i2c/spi/mmc/eth 的 init+傳輸序列，逐一對照我的狀態機/sim。
