# Raspberry Pi 5 → Windows on ARM 驅動移植狀態清單

> 更新：2026-06-25　｜　硬體全清單見 `RPi5-Driver-Porting-Inventory.md`
>
> **近況（2026-06-25）**：Pi5 **Linux** 唯讀基準機已可 SSH（萃取硬體真相校正驅動，見 `20260625-0030-...md`）。
> WiFi/藍牙已從規劃推進到實作：**藍牙 Phase A(sim 35/35)+Phase B WDF glue 全 /kernel 乾淨**；
> **WiFi 控制平面(sim 25/25)+WHD port layer(cy_rtos/cyhal/sdbus glue)全 /kernel 乾淨**。
> 仍待 **Pi5 跑 Win11-ARM 開發目標**做功能驗證（載入/bring-up/實機）。

## ⚠️ 重要前提與交接模型（雙機分工）
**無實機 + 無 Windows-on-Pi5 韌體（UEFI ECAM/ACPI、RP1 PCIe 列舉）下，硬體驅動無法「功能完整」**（無法載入/驗證）。本專案採**雙機交接**：
- **x64 端（本機 Zen2/Win11）**：cross-compile 產出、把邏輯補齊、寫模擬 harness 驗證序列、寫 ACPI/INF。
  → 硬體驅動可推到 **🔵 邏輯完整** 天花板；純軟體交付物（ACPI/INF/inbox）可達 **✅ 完整完成**。
- **Pi5 端（日後 Pi5/Win11-ARM 實機）**：實機載入、KDNET 雙機 WinDbg 除錯、校時序/中斷/DMA。
  → 把 🔵 推到 **✅ 完整完成（功能驗證）**。

### 狀態定義（重要）
| 狀態 | 定義 | x64 可達？ |
|:----:|------|:----------:|
| ⬜ **尚待處理** | 未開始 | — |
| 🟡 **部分完成** | 只到框架註冊；主體子系統未寫（只 compile 未 link、缺 capture filter / dot11 等） | ✅ 可推進 |
| 🟢 **骨架完成** | 可建置的 ARM64 driver ＋ 可沿用硬體邏輯（暫存器/init 序列）已移植；**未實機驗證** | ✅ |
| 🔵 **邏輯完整** | 骨架上把完整命令/傳輸/中斷**狀態機**由 Linux 源碼移植齊全，並以 **x64 模擬 harness 驗證序列邏輯**；只差真硬體跑/校時序 ← **硬體驅動在 x64 的最高天花板** | ✅ |
| ✅ **完整完成** | **純軟體交付物**（ACPI/INF/inbox 綁定）：寫完＋編譯驗證即完成；**硬體驅動**：需實機載入＋功能驗證＋時序校正 | 純軟體 ✅／硬體需 Pi5 |
| ➖ **免驅動** | 由 ACPI/inbox 處理，不需自寫驅動 | ✅（寫 ACPI 即可） |

> **一句話**：**「骨架」**＝能 build、硬體邏輯部分移植；**「邏輯完整」**＝邏輯全移植＋x64 模擬過（差實機校正）；**「完整完成」**＝實機功能驗證過（純軟體交付物則寫完即算）。
> 故 **🟢/🔵 是 x64 端能交付的；✅（硬體類）由 Pi5 端接手完成。**

**總計**：🟢 骨架 5　｜　🔵 邏輯完整 18（…/watchdog/DMA/clocks/PIO/BCM2712-GPIO）　｜　🟡 部分 2　｜　✅ 完整 2（ACPI + 驅動 INF 15/15）　｜　⬜ 尚待 ~17（多屬需實機/消費者型）　｜　➖ 免驅動 5（含 UART/USB inbox）

---

## 🤝 交接盤點：x64 能做到哪、Pi5 接手什麼

### ✅ 能在 x64「完整完成」（純軟體交付物，無需實機）
| 項目 | 內容 | 現況→目標 |
|------|------|-----------|
| **ACPI 描述（`rp1.asl`）** | 補全所有 RP1 子裝置節點 + `PNP0A08 _CRS` + `GpioInt` 對應；`asl.exe` 編譯驗證 | ✅ **已補完**（~33 子裝置，asl.exe 編譯過） |
| **各驅動 INF** | PnP ID 綁定、安裝段、服務；`infverif` 驗證 | ✅ **15/15 全部 VALID** |
| **inbox 綁定（UART/USB/PCIe RC）** | 僅需 ACPI 描述（HID `ARMH0011`/`PNP0D10`/`PNP0A08`） | ➖ 隨 ACPI 完成即 ✅ |

### 🔵 能在 x64 推到「邏輯完整」（補完狀態機＋模擬驗證；Pi5 校時序）
| 項目（現 🟢） | x64 補什麼（Linux 源碼可移植） | Pi5 接手校驗 |
|------|------|------|
| ~~I2C/SPI ×4（#5–8）全 🔵~~ | **4 控制器 x64 模擬全過**（DW-I2C 17 / DW-SPI 16 / BCM-I2C 13 / BCM-SPI 12 = 58 斷言） | 真實時脈/匯流排電氣 |
| ~~GPIO（#9）~~ 🔵 | **中斷 enable/mask/query/clear x64 模擬 13/13**（CTRL IRQEN/INTE/INTS、3-bank） | MSI-X→GIC 路由、bus demux 需實機 |
| ~~SD/MMC（#10）~~ 🔵 | **命令引擎已補完 + x64 模擬 18/18 驗證**（`sdhci_hw.c` + `sim/sdhci_sim.c`） | 卡片時序、DMA 資料路徑（需實機）|
| ~~Ethernet（#11）~~ 🔵 | **GEM 引擎 x64 模擬 22/22**（descriptor/MAC/MDIO） | NBL→ring 接線、DMA buffer、PHY link 需實機 |
| ~~音訊 HAL（#1）~~ 🔵 ／ PortCls（#2） | **DW I2S 引擎 x64 模擬 20/20**；#2 DataRangeIntersection 待 | codec、真實 DMA、PortCls 端點 |
| ~~Bluetooth（#12）~~ 🔵 | **Phase A 邏輯**（H4 framing/RX parser/.hcd parser/vendor payloads/bring-up 狀態機）**x64 sim 35/35**；**Phase B WDF glue（B1-B5）全 ARM64 /kernel 編譯乾淨**：開 UART+GPIO IoTarget、用狀態機跑 bring-up、RX pump、`.hcd` 載入(ZwReadFile)、PnP/D0 接線、INF/ASL 草稿；Pi5 實機數據驗證 | 功能驗證需 Win11 實機；**上層走 inbox BthUart.sys**（`bthx.h` 不在現代 WDK），BthUart 組合方式待實機 |

### 🟡 需「大工程或缺件」（x64 可寫結構，完整功能仍需實機/額外專案）
| 項目 | 卡點 |
|------|------|
| GPU V3D（#13） | **UMD**（Mesa v3d 移植＝獨立大專案）＋ V3D MMU/CL 提交需實機 |
| WiFi（🟡→🟢 #6） | **改走 WHD+NetAdapterCx**（不寫 dot11/WDI）。SDIO 控制平面 A1-A3 **sim 25/25**；**WHD source 已取得**，port layer（`cy_rtos_win`/`cyhal_sdio_win`/**`sdbus_glue`** 接真 sdbus.sys）全 **ARM64 /kernel 乾淨**；Pi5 實機數據驗證。卡：KMDF DriverEntry+NetAdapterCx 組裝 + WHD 整合 + 韌體載入需實機 |
| 相機（🟡 #7） | capture filter/pin/datarange（可寫結構）＋ ISP/sensor 需實機 |
| 顯示 modeset、DSI/VEC、HEVC、IOMMU… | vc4 HVS/PixelValve 時序、DXVA 等需實機 |

> **下一步 x64 端建議優先序**：① ACPI 補完 + 各驅動 INF（→ ✅，部署綁定關鍵）→ ② 核心 HAL 推到 🔵 邏輯完整（SDHCI/Ethernet/I2C 先）+ 模擬 harness。
> **Pi5 端接手清單**：先決 = Windows-on-Pi5 開機（UEFI + PCIe RC + RP1 列舉 + ACPI）；之後逐一把 🔵 校驗成 ✅。

---

## 🟢 已完成（骨架／可建置，達無實機天花板）

| # | 驅動 | 模型 | 產出 | 已移植 | 餘下（需實機） |
|---|------|------|------|--------|----------------|
| 1 | **RP1 I2S 音訊（硬體層）** 🔵 | WDM + HAL | `windows_driver/audio/rp1i2s.sys` | DW I2S **完整引擎**（Probe 能力解碼/SetResolution/Config/Start/Stop/Flush）；**x64 模擬 20/20 驗證** | 接真實 MMIO、DMA、codec |
| 2 | **RP1 I2S 音效卡** | PortCls/WaveRT | `windows_driver/audio/rp1i2saud.sys` | adapter+wave+topo+stream 端點 | DataRangeIntersection、DMA、codec |
| 3 | **顯示（HDMI 點亮）** | WDDM Display-Only | `windows_driver/display/rp1vc4dod.sys` | 19 DDI + 單一 HDMI child | vc4 HVS/PixelValve/HDMI modeset |
| 4 | **PCIe/RP1 bus（先決命脈）** | KMDF bus + ACPI | `windows_driver/pcie-rp1/rp1bus.sys` + `rp1.aml` | 綁 PCI、map BAR1、列舉子裝置、ACPI(GpioInt) | raw resource 切片、GpioClx 中斷 demux、INF |
| 5 | **RP1 I2C（DesignWare）** 🔵 | SpbCx (KMDF) | `windows_driver/i2c/rp1i2c.sys` | DW I2C **完整傳輸狀態機**（Probe/ConfigureMaster/SetTarget/Write+STOP+TX_ABRT/Read+RFNE）；**x64 模擬 17/17 驗證** | SCL 時脈值需實機校正、中斷模式、SPB sequence |
| 6 | **RP1 SPI（DesignWare）** 🔵 | SpbCx (KMDF) | `windows_driver/spi/rp1spi.sys` | DW-SSI **完整全雙工引擎**（Probe/ConfigureMaster CTRLR0 mode0-3/SetCs/Transfer）；**x64 模擬 16/16 驗證** | 速率值需實機校正、中斷模式、SPB sequence |
| 7 | **BCM2712 I2C（BSC）** 🔵 | SpbCx (KMDF) | `windows_driver/i2c/bcm2712i2c.sys` | BSC **完整傳輸狀態機**（Begin/C-ST/S 輪詢 TXD/RXD/DONE/ERR/FIFO）；**x64 模擬 13/13 驗證** | 時脈分頻值需實機校正、target 解析、中斷模式 |
| 8 | **BCM2712 SPI** 🔵 | SpbCx (KMDF) | `windows_driver/spi/bcm2712spi.sys` | SPI **完整全雙工引擎**（CS TA/TXD/RXD/DONE 輪詢、mode0-3）；**x64 模擬 12/12 驗證** | 速率值需實機校正、中斷模式 |
| 9 | **RP1 GPIO（GpioClx）** 🔵 | GpioClx (KMDF) | `windows_driver/gpio/rp1gpio.sys` | RIO 讀寫/方向/FUNCSEL + **中斷 enable/mask/query/clear 全接上**（CTRL IRQEN/PCIe INTE/INTS、3-bank 聚合）；**x64 模擬 13/13 驗證** | MSI-X→GIC 實際路由、bus demux 接 #4（需實機）|
| 10 | **RP1/BCM2712 SD/MMC** 🔵 | SdPort miniport | `windows_driver/storage/rp1sd.sys` | 16 callback + **SDHCI 命令引擎**（reset/clock/power/cmd/resp/int/card-detect）接上；**x64 模擬 18/18 驗證** | DMA 資料路徑、tuning、voltage switch、PIO buffer 需實機 |
| 11 | **RP1 Ethernet（Cadence GEM）** 🔵 | NDIS 6.30 miniport | `windows_driver/net/rp1gem.sys` | 13 handler + **GEM 引擎**（reset/MAC/NCFGR/ring/TX-RX descriptor/MDIO，編入 .sys）；**x64 模擬 22/22 驗證** | NBL→descriptor 接線、DMA common buffer、PHY、general attributes 需實機/NDIS plumbing |
| 12 | **Bluetooth（BCM4345C0）** 🔵 | KMDF (UART H4 HCI) | `windows_driver/bluetooth/btbcm.sys` | **Phase A**（H4 framing/RX 重組/.hcd parser/baud+BD_ADDR payload/bring-up 狀態機）**sim 35/35**；**Phase B WDF glue B1-B5 全 /kernel 乾淨**（`uart.c`：UART+GPIO IoTarget、`BtBcmBringUp`、RX pump；`driver.c`：PnP/D0Entry/D0Exit；`BtBcmLoadFirmware` ZwReadFile；`btbcm.inf`+`bt.asl` 草稿）；Pi5 驗證（PL011@7d50c000、3M、GPIO29、.hcd FC4C）| 上層走 **inbox BthUart.sys**（`bthx.h` 不在現代 WDK，IOCTL 未公開）；功能驗證(M2 心跳起)需 Win11 實機 |
| 13 | **GPU V3D（VideoCore VII）render KMD** | WDDM render (DxgkInitialize) | `windows_driver/gpu/rp1v3d.sys` | **38 DDI 全部定義 + link**（DRIVER_INITIALIZATION_DATA、PnP/render/VidPn 全套） | V3D MMU/CL 提交/fence 引擎 + UMD（user-mode D3D）需實機 |
| 14 | **RP1 PWM** 🔵 | KMDF function | `windows_driver/pwm/rp1pwm.sys` | PWM 引擎（channel config/duty/range/enable，GLOBAL_CTRL 多通道）；**x64 模擬 10/10 驗證** | 時脈週期換算、IOCTL 介面、風扇/熱區整合需實機 |
| 15 | **BCM2712 RNG（iProc RNG200）** 🔵 | KMDF function | `windows_driver/rng/bcmrng.sys` | RNG 引擎（RBG enable/int clear/FIFO count/read word）；**x64 模擬 6/6 驗證** | CNG 熵池介接需實機 |
| 16 | **VideoCore mailbox（BCM2712）** 🔵 | KMDF function | `windows_driver/mailbox/bcmmbox.sys` | mailbox 引擎（send/recv + FULL/EMPTY 輪詢、channel 編解碼）；**x64 模擬 8/8 驗證** | property-tags 協定層、clock/power 子系統介接需實機 |
| 17 | **RP1 ADC + 溫度感測** 🔵 | KMDF function | `windows_driver/adc/rp1adc.sys` | ADC 引擎（enable/TS_EN、channel 選擇、one-shot convert 輪詢 READY、12-bit result）；**x64 模擬 8/8 驗證** | IOCTL/hwmon 介接、校正需實機 |
| 18 | **BCM2712 watchdog（PM）** 🔵 | KMDF function | `windows_driver/watchdog/bcmwdt.sys` | watchdog 引擎（start/ping/stop + PM_PASSWORD、ticks=secs<<16 clamp 15s、is-running/time-left）；**x64 模擬 11/11 驗證** | Windows watchdog stack 介接需實機 |
| 19 | **BCM2712 DMA** 🔵 | KMDF function | `windows_driver/dma/bcmdma.sys` | DMA 引擎（per-channel reset/enable/start CB/active/done/ack-int，16 ch）；**x64 模擬 10/10 驗證** | 控制區塊建構、channel 配置、中斷需實機 |
| 20 | **RP1 clocks（PLL）** 🔵 | KMDF function | `windows_driver/clk/rp1clk.sys` | PLL 引擎（power-up/FBDIV int+frac/PRIM post-div/wait LOCK）+ clock gate（CTRL ENABLE）；**x64 模擬 11/11 驗證** | 完整 clock tree/mux、頻率計算需實機 |
| 21 | **RP1 PIO** 🔵 | KMDF function | `windows_driver/pio/rp1pio.sys` | TX/RX FIFO HAL（4 SM、offset 映射）；**x64 模擬 7/7 驗證** | SM 程式化（指令/clkdiv/pinctrl）走 RP1 韌體 #16、DMA bounce 需實機 |
| 22 | **BCM2712 GPIO（brcmstb）** 🔵 | GpioClx (KMDF) | `windows_driver/gpio/bcm2712gpio.sys` | GIO HAL（read/write/dir/中斷 EC/EI/MASK/LEVEL/STAT、active=STAT&MASK）+ 16 GpioClx callback 全接上（2 bank×32）；**x64 模擬 12/12 驗證** | 真實時脈/中斷路由需實機 |
| 23 | **Raspberry Pi RTC（韌體型）** 🟢 | KMDF（薄骨架） | `windows_driver/rtc/rpirtc.sys` | KMDF 骨架（**無 MMIO**）；時間 get/set 走 VC 韌體 property（mailbox #16） | 韌體 GET_RTC/SET_RTC property 交換需 mailbox + 實機 |

## ✅ 完整完成（x64 純軟體交付物，無需實機）

| 項目 | 內容 | 產出 |
|------|------|------|
| **ACPI 描述（SSDT）** | PNP0A08 host bridge + RP1 PCIe 子裝置（UART×6/I2C×7/SPI×6/I2S×3/audio/PWM×2/ADC/PIO/CLK/GPIO/ETH/CSI×2/DMA/USB×2，GpioInt=RP1 IRQ）**＋ 9 個 BCM2712 SoC 裝置**（SD/eMMC×2、mailbox、RNG、watchdog、DMA、GPIO、RTC、BSC-I2C×2、SPI；QWordMemory 真實位址 + GIC GSIV）| `windows_sources/pcie-rp1/acpi/rp1.aml`（asl.exe, **4529 B**；經 Pi5 實測校正位址/IRQ）|
| **驅動 INF（25/25 全部）** | 每個 `.sys` 都有對應 INF（先前 15 + 本輪補 10：rp1adc/rp1pwm/rp1pio/rp1clk/bcmmbox/bcmrng/bcmwdt/bcmdma/bcm2712gpio/rpirtc）；PnP ID 對齊 ACPI HID | `windows_driver/<類別>/*.inf`（**infverif 全 VALID**）|
| **Pi5 實測校正（loop d8511a69）** | 依真 BCM2712+RP1 矽晶片校正 7 處錯誤規格：USB IRQ、RP1 I2C SCL(200MHz)、SDHCI base+歸屬、RP1 SPI BaudDiv、I2S CCR、BCM SPI ClkDiv(750MHz)、BCM I2C DIV(72→100kHz/108MHz)；sim 全綠 | `MD/Note/20260621-0720-pi5-linux-hardware-facts.md` |

## 🟡 部分完成

| # | 驅動 | 狀態 | 位置 | 餘下 |
|---|------|------|------|------|
| 5 | **WiFi（CYW43455）** 🟢 | **改走 WHD+NetAdapterCx 偽裝乙太網卡**（棄 NDIS/dot11）。`sdio_core.c`：讀 Chip ID 0x4345 + NVRAM 預處理 + resource feeder，**sim 25/25**；**WHD 取得 + port layer 全 /kernel 乾淨**：`whd_port/cy_rtos_win.c`(OSAL)、`cyhal_sdio_win.c`(HAL)、**`sdbus_glue.c`**(SDIO_OPS 接真 sdbus.sys、`SdBusSubmitRequest`)、`cyw43455.inf`；Pi5 驗證（F1 sig 0x15264345、真 nvram 2074→1743B）| KMDF DriverEntry/EvtDeviceAdd + NetAdapterCx 組裝 + WHD 整合 + 韌體載入需實機（見 `wifi/cyw43455/PLAN.md`）|
| 6 | **相機 CSI（RP1 CFE/PiSP）** | AVStream minidriver 註冊骨架（C++, ks.sys） | `windows_driver/camera/rp1cfe.sys` | capture KSFILTERFACTORY（CSI-2 sensor→ISP→video pin）、DMA/buffer 佇列、sensor I2C 控制 |

---

## ⬜ 尚待處理

### A. RP1 周邊（PCIe 底下，靠 #4 bus driver 列舉後各自綁定）
| 驅動 | Linux 來源 | Windows 框架 | 備註 |
|------|-----------|--------------|------|
| ~~RP1 UART ×6~~ | `amba-pl011.c`(axi) | SerCx2 | ➖ Windows inbox `SerPl011.sys`（ACPI `ARMH0011`）；AXI 變體待實機確認 |
| ~~RP1 GPIO / pinctrl~~ | `pinctrl-rp1.c` | **GpioClx** | ✅ 見 🟢 #9；RIO 讀寫/方向/FUNCSEL 已接；餘中斷 demux（需實機）|
| ~~RP1 I2C ×7~~ | `i2c-designware-platdrv.c` | SpbCx | ✅ 控制器驅動見 🟢 #5（`rp1i2c.sys`）；7 實例各掛 ACPI 節點 |
| ~~RP1 SPI ×8~~ | `spi-dw-mmio.c` | SpbCx | ✅ 控制器驅動見 🟢 #6（`rp1spi.sys`） |
| ~~RP1 Ethernet (GbE)~~ | `macb_main.c` | NDIS 6.x | ✅ 見 🟢 #11（`rp1gem.sys`）；Cadence GEM |
| ~~RP1 USB3 ×2~~ | `dwc3/core.c` | inbox xHCI | ➖ dwc3 host=xHCI 相容 → Windows inbox `USBXHCI.sys`（ACPI `PNP0D10`）|
| RP1 DMA | `dw-axi-dmac` | KMDF DMA | 不同控制器（DW AXI），可套 #19 DMA pattern（換暫存器） |
| ~~RP1 PWM ×2~~ | `pwm-rp1.c` | KMDF | ✅ 見 🟢 #14（`rp1pwm.sys`）；🔵 sim 10/10 |
| ~~RP1 ADC~~ | `hwmon/rp1-adc.c` | KMDF | ✅ 見 🟢 #17（`rp1adc.sys`）；🔵 sim 8/8 |
| ~~RP1 SD/MMC ×2~~ | `sdhci-of-dwcmshc.c` | sdport | ✅ 見 🟢 #10（`rp1sd.sys`）；BCM2712 SD 可再套 |
| ~~RP1 PIO~~ | `misc/rp1-pio.c` | KMDF 自訂 | ✅ 見 🟢 #21（`rp1pio.sys`）；🔵 sim 7/7（FIFO；SM 走韌體）|
| RP1 類比音訊 out | `sound/soc/...` | PortCls | |
| ~~RP1 clocks~~ | `clk-rp1.c` | KMDF clock | ✅ 見 🟢 #20（`rp1clk.sys`）；🔵 sim 11/11 |
| RP1 firmware/mailbox | `firmware/rp1-fw.c` | KMDF | |
| RP1 相機 CSI ×2 | `media/.../rp1_cfe` | AVStream | 🟡 minidriver 註冊骨架（見 🟡 #7）；capture filter 未完 |
| RP1 DSI ×2 / DPI / VEC | `drm/rp1/...` | WDDM | 顯示輸出 |

### B. BCM2712 SoC 平台裝置
| 驅動 | Linux 來源 | Windows 框架 | 備註 |
|------|-----------|--------------|------|
| VideoCore 韌體介面 | `firmware/raspberrypi.c` | KMDF(mailbox) | clock/power 核心相依 |
| BCM2712 clocks | `clk-raspberrypi.c` | 併入韌體 | |
| ~~BCM2712 GPIO~~ | `gpio-brcmstb.c` | GpioClx | ✅ 見 🟢 #22（`bcm2712gpio.sys`）；🔵 sim 12/12 |
| ~~BCM2712 pinctrl~~ | `pinctrl-brcmstb-bcm2712.c` | GpioClx | 併入 #22（GpioClx 含方向/功能）|
| SD 卡 (SDHCI) | `sdhci-brcmstb.c` | sdport | 開機媒體 |
| ~~BCM2712 I2C (BSC)~~ | `i2c-bcm2835.c` | SpbCx | ✅ 見 🟢 #7（`bcm2712i2c.sys`）；brcmstb 變體可再套 |
| ~~BCM2712 SPI~~ | `spi-bcm2835.c` | SpbCx | ✅ 見 🟢 #8（`bcm2712spi.sys`） |
| ~~BCM2712 DMA~~ | `bcm2835-dma.c` | KMDF DMA | ✅ 見 🟢 #19（`bcmdma.sys`）；🔵 sim 10/10 |
| IOMMU | `bcm2712-iommu.c` | DMA remapping | |
| ~~Mailbox (VC)~~ | `bcm2835-mailbox.c` | KMDF | ✅ 見 🟢 #16（`bcmmbox.sys`）；🔵 sim 8/8 |
| ~~RNG~~ | `iproc-rng200.c` | KMDF | ✅ 見 🟢 #15（`bcmrng.sys`）；🔵 sim 6/6 |
| 溫度/AVS | `bcm2711_thermal.c` | ACPI 熱區 | |
| ~~Watchdog/PM~~ | `bcm2835_wdt.c` | KMDF | ✅ 見 🟢 #18（`bcmwdt.sys`）；🔵 sim 11/11 |
| HDMI ×2 (vc4) | `vc4_hdmi.c` | WDDM | 與 #3 Stage B 整合 |
| HVS/PixelValve/MOP | `vc4_*` | WDDM | |
| HEVC 解碼 | `hevc_d.c` | MFT/DXVA | |
| ISP (PiSP) | `pisp_be.c` | AVStream | |
| USB OTG (內建) | `dwc2/` | inbox/自訂 | 次要 |

### C. 外接／板載晶片
| 驅動 | Linux 來源 | Windows 框架 | 備註 |
|------|-----------|--------------|------|
| WiFi (CYW43455) | `brcmfmac` | **WHD + NetAdapterCx** | 🟢 sim 25/25；WHD port + sdbus glue /kernel 乾淨；改走 WHD 偽裝乙太網（見 #5 + `wifi/cyw43455/PLAN.md`）|
| ~~Bluetooth (4345C0)~~ | `hci_bcm.c` | **inbox BthUart.sys** | 🔵 見 #12；Phase A sim 35/35 + Phase B WDF glue /kernel 乾淨 + Pi5 驗證；`bthx.h` 不在現代 WDK→走 BthUart |
| Ethernet PHY | `net/phy/broadcom.c` | 併入 NDIS | |
| 風扇 (PWM) | `pwm-fan.c` | KMDF+熱區 | |
| 電源鍵 | `gpio_keys.c` | HID 按鈕 | 消費者型（GPIO pin→HID/ACPI button），非 register-HAL |
| 狀態 LED | `leds-gpio.c` | KMDF | 消費者型（用 GPIO pin，非 register-HAL）；走 GpioClx 消費者 |
| ~~RTC (韌體)~~ | `rtc-rpi.c` | KMDF/ACPI | ✅ 見 🟢 #23（`rpirtc.sys`）；韌體型走 mailbox #16 |

---

## ➖ 免驅動（ACPI/韌體處理）
| 項目 | 處理方式 |
|------|----------|
| BCM2712 PCIe Root Complex | UEFI 韌體出 ECAM(MCFG/PNP0A08)，inbox `pci.sys` |
| GIC-400 中斷控制器 | ACPI MADT（OS 內建） |
| Arch/System Timer | ACPI GTDT |
| PMIC | VideoCore VPU 韌體管理 |
| UART (PL011) | Windows inbox `SerPl011.sys`，ACPI `_HID ARMH0011` |
| USB3 ×2 (dwc3 host) | Windows inbox `USBXHCI.sys`，ACPI `_HID PNP0D10`（xHCI 相容）|

---

## 進度索引（細節筆記）
- 音訊：`20260621-0331`(P1)、`20260621-0341`(P2)
- 顯示：`20260621-0347`、`-0352`、`-0358`
- GPU：`20260621-0405`
- **PCIe/RP1/ACPI（先決命脈）**：`20260621-0415`
- 硬體全清單：`RPi5-Driver-Porting-Inventory.md`
