# Skill：Pi5 Win11-ARM 實機 bring-up 順序總表（playbook）

> 接上 **Pi5 跑 Win11 ARM64 開發目標**（SSH）後，照本表從第一關往下攻：把 x64 端已做到 🔵（HAL+sim）/
> 骨架的驅動推到 **✅ 功能驗證**。每列：**先決 → 第一驗收點 → 已備 HAL/sim → 誰做**。
> 圖例：🤖=我可透過 SSH 自動跑（build→scp→載入→測→收 log）｜🧑=需要你（拔電救援/目視/互動除錯/動韌體）。
> 建立：2026-06-25。原則見 `MD/Note/RPi5-Porting-Status.md`（雙機交接模型）。

---

## Phase 0 — 環境與先決（最先確認）
| # | 項目 | 怎麼確認 | 誰 |
|---|------|----------|----|
| 0.1 | Windows 能在 Pi5 開機到桌面（哪怕只有 inbox 驅動）| RDP/螢幕看到桌面；`Get-ComputerInfo` 確認 ARM64 | 🧑 |
| 0.2 | SSH 通 + 預設 shell=PowerShell | `ssh user@pi5 "$PSVersionTable"` | 🧑 開 / 🤖 測 |
| 0.3 | `bcdedit /set testsigning on` + 自動 small crash dump | `bcdedit /enum` 看 testsigning；`%SystemRoot%\Minidump` | 🤖 設 |
| 0.4 | **救援後路**（壞驅動可能開不了機）：還原點 / 乾淨開機項 / SD 卡備份 | — | 🧑 |
| 0.5 | （需互動除錯時）KDNET 雙機 WinDbg | `bcdedit /dbgsettings net` | 🧑 |

> **關鍵閘門**：若 0.1 還沒到（Windows 上不了 Pi5），那就要先一起攻 **UEFI + ACPI + PCIe RC**（部分要動韌體/SD 卡，非 SSH 能解）→ 見 Phase 1。

## Phase 1 — 命脈：UEFI / ACPI / PCIe / RP1 列舉
| # | 項目 | 第一驗收點 | 已備 | 誰 |
|---|------|-----------|------|----|
| 1.1 | UEFI + ACPI 表（PNP0A08 host bridge + RP1 子裝置 + BCM2712 SoC 裝置）| 裝置管理員看到 PCIe RC + RP1 節點無錯 | `windows_sources/pcie-rp1/acpi/rp1.aml`（已寫，4529B）| 🧑 刷韌體 / 🤖 改 ASL |
| 1.2 | **IOMMU/SMMU**：ACPI IORT（BCM2712 MMU-500）| DMA 不觸發 SMMU fault | 筆記 `MD/Note/.../iommu`（要寫進 UEFI）| 🧑 |
| 1.3 | **RP1 PCIe bus driver**（命脈）：BAR1 映射 + 動態列舉子 PDO + bus interface | RP1 底下 GPIO/I2C/… **列舉成功**且能查 `GUID_RP1BUS_INTERFACE_STANDARD` 取窗口/IRQ | `pcie-rp1/rp1bus`（**bind PCI+map BAR1+WDFCHILDLIST+介面，ARM64 link 乾淨**；INF 後裝）；**MSI-X→內部 IC demux 仍需實機** | 🧑+🤖 |
> ⚠️ **1.3 不通，下面所有 RP1 周邊都列舉不到** → 這是最硬的一關，要一起攻（PCIe link up / ECAM / IORT）。

## Phase 2 — 標準周邊（低風險，🤖 可大量自動推 ✅）
> 列舉成功後，這些 HAL 都已 sim 過，實機多半只差「校時脈/中斷/DMA」。逐一 deploy→load→IOCTL 測→收 log。

| 順序 | 裝置 | 第一驗收點 | 已備 HAL/sim |
|------|------|-----------|--------------|
| 2.1 | **GPIO**（RP1 + BCM2712）| 讀回 pin 狀態 / set 一根 high 量到電壓 | `gpio/rp1-gpio`(13)、`gpio/bcm2712-gpio`(12) |
| 2.2 | **clk**（RP1）| 讀回時脈樹值 | `clk/rp1-clk`(11) |
| 2.3 | **I2C ×2**（RP1 DW + BCM2712）| 對已知 I2C 裝置（如 PMIC/EEPROM）讀回暫存器 | `i2c/rp1-dw-i2c`、`i2c/bcm2712-i2c`(17/13) |
| 2.4 | **SPI ×2** | loopback（MOSI→MISO 短接）讀回送出值 | `spi/rp1-dw-spi`、`spi/bcm2712-spi`(16/12) |
| 2.5 | **UART PL011** | TX 一串字元、RX loopback | `uart/rp1-pl011`(18) |
| 2.6 | **RNG** | 讀回非零亂數、通過 warmup | `rng/bcm2712-rng`(6) |
| 2.7 | **watchdog** | 設 timeout、餵狗、確認不重啟；不餵→重啟 | `watchdog/bcm2712-wdt`(11) |
| 2.8 | **PWM / ADC** | PWM 出波形量 duty；ADC 讀回已知電壓 | `pwm/rp1-pwm`(10)、`adc/rp1-adc`(8) |
| 2.9 | **RTC** | 讀/設時間（mailbox）| `rtc/rpi-rtc` |
| 2.10 | **mailbox**（命脈相依）| 讀回韌體版本/序號 | `mailbox/bcm2712-mailbox`(8) |
| 2.11 | **DMA**（要 IOMMU 1.2 先通）| 一筆 mem-to-mem 傳輸正確 | `dma/bcm2712-dma`(10) |
| 2.12 | **PIO**（RP1）| FIFO 推送/讀回 | `pio/rp1-pio`(7) |
| 2.13 | **SDHCI / SD-MMC** | 掛載 SD 卡、讀寫 sector | `storage/rp1-sdhci`(18) |
| 2.14 | **Ethernet (GEM)** | link up、PHY 通、ping 通 | `net/rp1-gem`(22) |
| 2.15 | **I2S 音訊** | PortCls 端點出現、播放 PCM | `audio/rp1-i2s`(20) + portcls |
| 2.16 | **溫度感測** | 讀回合理 °C（手摸/負載升溫）| `thermal/bcm2712-avs`(7) |

## Phase 3 — 顯示 DOD（點亮畫面）🤖+🧑(目視)
| 里程碑 | 驗收 | 已備 |
|--------|------|------|
| M1 假顯示 | RDP 看到 1080p PnP 螢幕（EDID）| `display/rp1-vc4-dod`（EDID fallback）|
| M2 軟體 VSync + WARP | 系統不卡、WARP 給 framebuffer | DOD 骨架 |
| **M3 UEFI GOP 劫持** 🧑目視 | **螢幕出現 Windows 桌面**（memcpy 到 GOP fb）| DOD + mailbox builder |
| M4 HVS 硬體 flip | 流暢、CPU 降載 | `hvs_dlist.c`(寫 SCALER_DISPLISTX) |

## Phase 4 — 無線（會 BSOD，🧑救援+目視）
| 裝置 | 第一驗收點 | 已備 |
|------|-----------|------|
| **藍牙** | 拉高 BT_REG_ON→送 HCI_Reset→收 `04 0E 04 01 03 0C 00`（晶片活）→ 載 BCM4345C0.hcd → bthport 出現 "Bluetooth Radio" → 配對滑鼠 | `bluetooth/bcm43438`(35) + uart.c glue + INF/ASL |
| **WiFi** | sdbus 列舉 CYW43455 → CMD52 讀 Chip ID **0x4345** → WHD 載韌體(polling) → NetAdapterCx → join AP → ping | `wifi/cyw43455`(25) + whd_port(sdbus_glue) + WHD source |

## Phase 5 — 多媒體 / 其餘（核心邏輯已備，接框架）
| 裝置 | 第一驗收點 | 已備 |
|------|-----------|------|
| **HDMI 音訊** | WaveRT 端點出現、ACR 設定、喇叭出聲（M3「嗶」）| `hdmi-audio`(12) + DOD private interface |
| **HEVC 解碼** | rpivid 解一張 I-frame → SAND→NV12 → 存 .yuv 對 | `hevc/sand.c`(6) + KMDF/MFT |
| **相機** | sensor I2C 讀 Chip ID 0x0708 → CFE DMA 收 RAW → DeviceMFT debayer → 相機 App 有畫面 | `camera/isp`(11) + AVStream + cfe |
| **HDMI CEC** | 送 <Active Source>、收遙控鍵 | `hdmi-cec`(13) |
| **PMIC / 電源鍵** | 讀電壓、按電源鍵觸發事件 | `pmic/da9090`(10) |
| **OTP** | 讀回序號/MAC | `otp/rpi-otp`(12) |

## Phase 6 — 大工程（人年級 / 需大量實機）
- **V3D GPU render**：精簡 KMD（記憶體/CLE/中斷）+ Mesa(v3d/v3dv) port + DXVK → 玩家 3D 加速。先驗「硬體 clear buffer 成紅色」。`gpu/`(骨架+暫存器) + `MD/Note/gpu/`。
- **PiSP 硬體 ISP / VCHIQ / DSI/VEC 顯示輸出**：各自獨立大塊，依需求再開。

---

## 每個驅動的標準 bring-up 迴圈（🤖 跑）
```
1. 本機交叉編譯 ARM64 .sys（build.ps1）
2. scp .sys + .inf → Pi5
3. ssh pnputil /add-driver xxx.inf /install   (或 sc create + sc start)
4. 觸發（IOCTL / 對應動作）→ 收 log（WPP / DbgView / event log）
5. 失敗→拉 crash dump 回本機判讀（!analyze）→ 修 → 回 1
6. 過了→更新 RPi5-Porting-Status.md 該列 🔵→✅
```

## 我做不到、要你的
- 硬當機後**拔電重開**；UEFI/ACPI **刷韌體**（不在 Windows 內）；**互動式 WinDbg**（我給指令你操作）；**目視/聽覺驗收**（畫面/聲音/燈）。
