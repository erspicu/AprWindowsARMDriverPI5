# x64 端「邏輯完整(🔵)+模擬」loop 總結（job 5d19ab08 解除）

> 日期：2026-06-21 06:00　｜ loop「盡量移植到 x64 能完成的階段」完成並解除

## 本 loop 達成
### ✅ 完整完成（純軟體交付物，無需實機）
- **ACPI 描述** `rp1.aml`：PNP0A08 + RP1 + ~33 子裝置（GpioInt=內部 IRQ + _HID/_DSD），asl.exe 編譯過。
- **驅動 INF 15/15**：每個 .sys 一個，PnP ID 對齊 ACPI HID，**infverif 全 VALID**。

### 🔵 邏輯完整（HAL 全移植 + x64 模擬驗證；差實機校時序）
| # | 引擎 | 模擬斷言 |
|---|------|---------|
| SDHCI（SD/MMC #10） | reset/clock/cmd/resp/int/card-detect | 18 |
| DesignWare I2C（#5） | Probe/Configure/SetTarget/Write+STOP/Read+RFNE | 17 |
| DesignWare SPI（#6） | Probe/Configure CTRLR0/SetCs/全雙工 | 16 |
| BCM BSC I2C（#7） | Begin/C-ST/S 輪詢 TXD/RXD/DONE/FIFO | 13 |
| BCM SPI（#8） | CS TA/TXD/RXD/DONE 全雙工 | 12 |
| Cadence GEM Ethernet（#11） | reset/MAC/NCFGR/ring/TX-RX desc/MDIO | 22 |
| DesignWare I2S 音訊（#1） | Probe 能力解碼/Config/Start/Stop/Flush | 20 |
| RP1 GPIO 中斷（#9） | enable/mask/unmask/query/ack、3-bank | 13 |
| BCM43438 BT H4（#12） | H4 framing opcode/param/邊界、BCM init | 12 |
| **合計** | **9 引擎** | **143** |

## 可複用方法（🔵 pattern）
1. **HAL 抽暫存器狀態機**（不依賴 OS 型別，poll 加 bounded retry）。
2. **reg-I/O shim**：`#ifdef <X>_SIM` → mock；否則 `READ/WRITE_REGISTER_*`。對既有用 ntddk 型別的 HAL，改用「fake kernel」simshim（提供 UINT32/PUCHAR/NTSTATUS/RtlXxx…）+ header 條件 include。
3. **sim harness**：mock 暫存器陣列 + 在寫入時模型化裝置 side-effect（reset 自清/FIFO/STOP/W1C…），跑序列 + `check()` 斷言。
4. **接 OS 層**：framework callback（SdPort/SpbCx/NDIS/GpioClx）翻譯成 HAL 呼叫。
5. driver build 非遞迴 glob（`sim/` 自動排除）；sim 用 host x64 cl（user-mode include）。

## x64 端天花板已達 — 剩下交給 Pi5 端 / 大工程
- **需實機**：各引擎真硬體跑/校時序/中斷路由（MSI-X→GIC）、DMA、PCIe bus raw-resource+demux、DOD vc4 modeset。
- **大型框架（x64 可寫結構但非 sim-able 狀態機）**：WiFi dot11/WDI、camera capture filter、PortCls #2 DataRangeIntersection、GPU V3D **UMD（Mesa v3d 移植）**。
- **缺 SDK**：BT bthport 整合需 `bthx.h`（此 WDK 無）。

## 全專案狀態
🟢 骨架 4 ｜ 🔵 邏輯完整 9 ｜ 🟡 部分 2（WiFi/camera）｜ ✅ 完整 2（ACPI/INF）｜ ➖ 免驅動 5。詳見 `RPi5-Porting-Status.md`。
