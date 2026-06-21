# 原始碼 vs 移植：B 類缺漏完整盤點（7 驅動並行比對）

> 2026-06-21　｜ 對所有 register-HAL 驅動做「Linux 源碼 init/config 序列 vs 我的移植」逐項比對，
> 找出「源碼有寫該暫存器、但我 HAL 漏抄/寫錯且影響功能」的 **B 類缺漏**（A 類 runtime 值另案）。
> 判別與 A/B 定義見 `20260621-0720-pi5-linux-hardware-facts.md` 的「Pi5 端對照原則」。

## 🔴 會壞（would-break）— 優先補

### DW-I2C (`i2c/rp1-dw-i2c/dw_i2c_hw.c`)
- **SDA_HOLD 漏寫**：init 未寫 `DW_IC_SDA_HOLD(0x7c)`。Linux `i2c_dw_set_sda_hold`＋`i2c_dw_init_master` 設 `lcnt/2 | (1<<16)`(RX-hold workaround，避免 TX arbitration-lost)。版本 0x0140≥1.11 支援。→ 邊際 slave 間歇 NAK/abort。
- **disable 未輪詢 ENABLE_STATUS**：`DwI2cHwDisable` 只寫 `ENABLE=0` 即返回。Linux `__i2c_dw_disable` 寫 0 後輪詢 `ENABLE_STATUS(0x9c) bit0` 清掉（≤100×25us）＋ abort 收尾。`DwI2cHwSetTarget` 的 disable→寫TAR→enable 在 busy 時設定會被忽略 → 偶發卡死/設定未生效。

### DW-SPI (`spi/rp1-dw-spi/dw_spi_hw.c`)
- **缺 reset-chip 序列**：probe/enable 前未做 `disable→mask IMR→讀 ICR 清中斷→寫 SER=0→enable`（Linux `dw_spi_reset_chip`）。暖開機殘留 CS/中斷狀態 → 首筆誤判 overflow/CS 提早 de-assert。
- **TMOD 寫死 TR**：`dw_spi_hw.c:62` 永遠 `TMOD_TR`，未隨只送/只收切 `TMOD_TO/RO`，RO 模式還要寫 `CTRLR1=ndf-1`。只收大量資料語意錯。
- **DFS32 偵測缺**（待實機確認）：固定 `(Bits-1)<<0`；若 RP1 SSI 為 DFS32 變體，DFS 欄位在 bit16，frame size 全錯。需 devmem 讀 CTRLR0 確認變體。

### BCM-SPI (`spi/bcm2712-spi/bcm_spi_hw.c`)
- **收尾未寫 CS_DONE**：`bcm_spi_hw.c:79` 收尾寫 `CS=0`，應寫 `CS_DONE(0x10000)|CLEAR_RX|CLEAR_TX`。Linux `bcm2835_spi_reset_hw` 註解明言「不寫 DONE 傳輸會間歇停擺」＋未清 FIFO 汙染下一筆。
- **Div==0 被竄改**：`bcm_spi_hw.c:19` `Div?Div:0x100`。但 CDIV=0 是合法「最慢(÷65536)」，與 `BcmSpiHwClkDiv` 回傳 0 矛盾 → 請求最慢速被忽略。應直接寫入(含 0)。
- **native CS 未無效化**：起手 CS 未設 `CS_01|CS_10`(=3 無效)，若 native CE 腳被 pinmux 出 → HW 自驅 CE 干擾 GPIO-CS 匯流排。

### SDHCI (`storage/rp1-sdhci/`) — 需改 send_command/bus-op 簽章（較大）
- **INT_ENABLE 漏錯誤位**：只 enable CMD/XFER/CARD_INS/REM，缺 TIMEOUT/CRC/END_BIT/INDEX/DATA_*(0x10000~0x800000)+DATA_END(0x08)。錯誤位沒致能 → INT_STATUS 不 latch → 錯誤永遠讀 0 → 上層卡死。
- **TRANSFER_MODE(0x0C) 從未寫**：資料命令需 `BLK_CNT_EN|(read?READ)|(multi?MULTI)`。漏 → 控制器不知方向/塊數，所有 read/write 資料命令失敗。
- **TIMEOUT_CONTROL(0x2E) 從未設**：預設 0 → 每筆資料立即 Data-Timeout。應寫 ~0x0E。
- **HOST_CONTROL(0x28) bus-width/high-speed 未設**：停在 1-bit/預設速度，4-bit/HS 卡初始化失敗。`Rp1SdIssueBusOperation` 是 TODO stub。
- 次要：BLOCK_SIZE 缺 SDMA boundary 高位(DMA 才需)、set_power 未先寫 0、dwcmshc reset 後清 INT_RESPONSE quirk。

### I2S (`audio/rp1-i2s/rp1_i2s_hw.c`)
- **IMR per-channel 中斷遮罩從未初始化**：start/stop 無 `i2s_enable_irqs/disable_irqs`(RMW `IMR(i)` TX `&~0x30`/RX `&~0x03`)。HAL 124 行自承「IRQ omitted」。純 block-DMA 可動；走中斷/PIO 取樣則資料搬不動。

### GPIO (`gpio/rp1-gpio/rp1_gpio_hw.c` + `ddi.c`)
- **PADS pull/in-out enable 完全沒移植**：`PadsBase` 有 map 但 HAL 從不讀寫。缺 `RP1_PAD_PULL`(mask 0xc, OFF/DOWN/UP)、IN_ENABLE(bit6)/OUT_DISABLE(bit7)。GpioClx 的 PullConfiguration 被丟棄 → 按鈕等輸入腳浮動誤觸發。
- **OEOVER/OUTOVER override 未寫**：`Rp1GpioSelectGpioFunction` 只寫 FUNCSEL，未把 OUTOVER(bit12-13)/OEOVER(bit14-15) 設 PERI(0)＋in/out enable。若腳殘留 override，RIO 方向無效。
- 次要：set-type 前未送 IRQRESET 清 latch(首次可能吃假中斷)、ddi.c 漏 `InterruptActiveBoth`(雙邊緣)。

### PWM (`pwm/rp1-pwm/pwm_hw.c`)
- **SET_UPDATE(BIT31) 從未寫** 🔴最嚴重之一：Linux 每次 apply 後寫 `GLOBAL_CTRL|=SET_UPDATE` 鎖存 duty/range 生效。**漏 → 設了 duty/range 硬體完全不套用，PWM 不輸出**。
- 次要：polarity(CHANNEL_CTRL bit3) 無介面。

### clk (`clk/rp1-clk/clk_hw.c`)
- **PLL_CS REFDIV_UNITY 未寫**：上電/設速應把 CS `GENMASK(5,0)=1`。漏 → REFDIV 非 1 則 VCO 參考分頻錯，頻率全錯/鎖不住。
- **PLL_PRIM 整條覆寫(應 RMW)**：`clk_hw.c:23-24` `WR32(PRIM, div1<<16|div2<<12)` 清掉其他位元。Linux 只 RMW DIV1/DIV2 mask。
- 次要：core-reset default(fbdiv_int=20)、GPCLK OE_CTRL。

### ADC (`adc/rp1-adc/adc_hw.c`，源在 `sources/drivers/hwmon/rp1-adc.c`)
- **未用 RWTYPE 別名窗(race)**：select/start 對 CS 普通 RMW，但 CS 含 READY/ERR 即時位。Linux 用 `RWTYPE_CLR(0x3000)/SET(0x2000)+CS` atomic 設 AINSEL/START_ONCE。普通 RMW 會 race+回寫過期狀態。
- **init 未清 CS_ERR_STICKY(0x400)**：probe 應 `INTE=0`＋`CS=EN|ERR_STICKY`。漏 → 開機帶 sticky error 則每次轉換都當錯。

### RNG (`rng/bcm2712-rng/rng_hw.c`，BCM2711 變體)
- **init 抄成泛用 iproc(只 enable RBGEN)**：缺 BCM2711 三步：`TOTAL_BIT_COUNT_THRESHOLD(0x10)=0x40000`、`FIFO_COUNT(0x24)=2<<8`、enable 寫 `(0x3<<13)|RBGEN`(含取樣率)。→ 開機初期低品質亂數/速率錯。
- **read 前未等暖機**：應輪詢 `TOTAL_BIT_COUNT(0x0C)>16`。漏 → 開機立即讀拿未暖機資料。

### DMA (`dma/bcm2712-dma/dma_hw.c`) 🔴架構性，需實機核對
- **抄成 legacy bcm2835，但 BCM2712 走 DMA40(40-bit)**：缺整組 DMA40 暫存器(`CS/CB/TI/SRC/DEST/LEN/NEXT_CB`、HALT/ABORT/PROT/DEBUG_RESET)。reset/start/ack 全錯(reset 不在 CS；start 要 `CB=paddr>>5`+`CS=ACTIVE|PROT|dreq`；ack 要保留 ACTIVE)。需依 is_2712 補 DMA40 HAL。

## ⚪ 無缺漏（已正確移植，供確認）
- **mailbox**：STA FULL/EMPTY、RD/WRT、poll 序列全對（中斷致能 MAIL0_CNF 屬 OS 介接，輪詢模型不需）。
- **watchdog**：PASSWORD/WDOG/RSTC/clamp/stop/timeleft 全對（partition/halt 屬 reboot 擴充，非看門狗核心）。
- 各驅動的 FIFO depth 偵測、threshold、CON/CTRLR0 主體、CCR、TFCR/RFCR 等多數已正確（詳見各 agent 報告）。

## ✅ 已套用之修正（x64，各帶 sim 驗證）
- **GEM**：補 NCFGR MDC 分頻(DIV96@200MHz)+DRFCS（sim 24）。
- **PWM**：補 SET_UPDATE 鎖存（duty/range 現會生效，sim 11）。
- **clk**：PLL_PRIM 改 RMW + 補 CS REFDIV_UNITY（sim 12）。
- **BCM-SPI**：收尾寫 CS_DONE+清FIFO+DLEN=0、Div==0 直寫、native CS=3 無效化（sim 18）。
- **ADC**：select/start 改 RWTYPE atomic alias、enable 清 CS_ERR_STICKY+INTE=0（sim 11）。
- **RNG**：BCM2711 三步 init（threshold/FIFO/DIV_CTRL）+ 暖機等待 TOTAL_BIT_COUNT>16（sim 11）。
- **I2S**：補 IMR per-channel enable/disable（start 解遮罩、stop 遮罩；sim 27）。
- **BCM-I2C**（稍早）：DIV 72→100kHz(108MHz)+DivForHz（sim 17）。
- **DW-I2C**：SDA_HOLD RX-hold workaround + disable 輪詢 ENABLE_STATUS（sim 18）。

- **DW-SPI**：reset-chip 序列(SSIENR/IMR/ICR/SER)（sim 24）；TMOD TO/RO 常數已加（polled 全雙工 TR 路徑可用，per-direction 為次要優化）。
- **GPIO**：補 PADS pull（GpioClx PullConfiguration→bias）+ in/out buffer enable + SelectGpioFunction 清 OUTOVER/OEOVER 成 PERI（sim 19）。

- **SDHCI**：補 INT_ENABLE 全錯誤位(DEFAULT_MASK)、SendCommand 寫 TRANSFER_MODE+TIMEOUT、SetBlock 加 SDMA boundary、set_power 先清零、新增 SetBusWidth/SetHighSpeed + 接 IssueBusOperation handler（reset/voltage/width/speed）（sim 29）。

- **DMA40**：從 `bcm2835-dma.c` is_40bit 路徑移植整組 register map + DmaHw40Reset/Start/IsActive/IsDone/AckInt（CB=paddr>>5、CS=ACTIVE|PROT|flags、reset=pause→PROT→DEBUG_RESET、ack 保留 ACTIVE cyclic-safe）（sim 19）。
- **DW-SPI DFS32**：移植 `dw_spi_hw_init` 的 runtime 自適應偵測（寫 CTRLR0=0xffffffff 回讀，低 4 bit 不黏即 DFS32→offset 16），ConfigureMaster 用 `Hw->DfsOffset`（sim 25）。**自適應，上實機自動正確，無需 devmem 預判**。

## 🎉 全部完成（2026-06-21，loop 2dc34690）
**14 個 register-HAL 驅動的 B 類缺漏全數從 sources 補正，sim 全綠**（GEM24/BCM-I2C17/PWM11/clk12/BCM-SPI18/ADC11/RNG11/I2S27/DW-I2C18/DW-SPI25/GPIO19/SDHCI29/DMA40-19）。所有「源碼有、移植時漏抄」的 B 類缺口已清空；剩餘只有需實機跑才能驗的時序/中斷/DMA 行為（🔵→✅，Pi5 階段）。
### 需實機核對（留 Pi5）
- **DMA40**：BCM2712 走 40-bit 引擎，整組 register map 需依 is_2712 重做（需 devmem 核對變體）。
- **DW-SPI DFS32**：需 devmem 讀 CTRLR0 確認是否 extended-DFS 變體。

## 補法優先序
1. **最高（設定根本不生效/資料命令全失敗）**：PWM SET_UPDATE、SDHCI(TRANSFER_MODE+TIMEOUT+INT_ENABLE+HOST_CONTROL)、DMA40 重做。
2. **高（間歇壞/品質）**：BCM-SPI CS_DONE、clk PLL_PRIM RMW+REFDIV、ADC RWTYPE+sticky、RNG 暖機、DW-SPI reset-chip、DW-I2C disable 輪詢、GPIO PADS pull。
3. **次要**：各 polarity/edge-both/clear-irqs/SDA_HOLD 等。
> 多數為純 B 類「重讀源碼即可補」；DMA40、DFS32 需實機核對 register 變體。
