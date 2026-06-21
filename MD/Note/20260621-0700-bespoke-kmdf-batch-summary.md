# bespoke KMDF 骨架批次總結（loop job e5d1fde0 解除）

> 日期：2026-06-21 07:00　｜「持續補 bespoke KMDF 骨架到 x64 極限」完成並解除

## 本 loop 產出（10 個驅動，9 個達 🔵 + 1 個 🟢）
| # | 驅動 | 來源 | 引擎 | sim |
|---|------|------|------|-----|
| 14 | RP1 PWM | `pwm-rp1.c` | channel config/duty/range/enable | 10/10 |
| 15 | BCM2712 RNG | `iproc-rng200.c` | RBG enable/FIFO/read | 6/6 |
| 16 | VideoCore mailbox | `bcm2835-mailbox.c` | send/recv FULL/EMPTY/channel | 8/8 |
| 17 | RP1 ADC | `rp1-adc.c` | enable/select/one-shot convert | 8/8 |
| 18 | BCM2712 watchdog | `bcm2835_wdt.c` | start/ping/stop + PASSWORD/clamp | 11/11 |
| 19 | BCM2712 DMA | `bcm2835-dma.c` | per-channel reset/start CB/done/ack | 10/10 |
| 20 | RP1 clocks | `clk-rp1.c` | PLL power/FBDIV/post-div/LOCK + gate | 11/11 |
| 21 | RP1 PIO | `rp1-pio.c` | TX/RX FIFO（SM 走韌體） | 7/7 |
| 22 | BCM2712 GPIO | `gpio-brcmstb.c` | GIO HAL + **16 GpioClx callback** | 12/12 |
| 23 | RPi RTC（韌體型 🟢） | `rtc-rpi.c` | 薄骨架（無 MMIO，走 mailbox #16） | — |

**本 loop 新增 83 條 x64 模擬斷言**（全專案累計 **18 個 🔵 引擎、~226+ 斷言**）。

## bespoke KMDF 模板（可複用）
`common.h`(ntddk+wdf+context) + `driver.c`(DriverEntry→WdfDriverCreate；EvtDeviceAdd 設 PnpPower callbacks；EvtPrepareHardware 找 `CmResourceTypeMemory`→`MmMapIoSpaceEx`) + `<dev>_hw.h/.c`(clean `void* Base` HAL + `<X>_regio.h` shim) + `sim/<dev>_sim.c`(mock + 斷言) + `build.ps1`(KMDF, entry FxDriverEntry)。

## x64 端到此真正耗盡
- **register-HAL 型**全數完成（上表 + 先前 9 個框架引擎）。
- **不適用 register-HAL 的剩餘項**（消費者/韌體/HID 型）：
  - LED（`leds-gpio`）、電源鍵（`gpio_keys`）：用 GPIO pin，走 GpioClx 消費者 / ACPI HID，非獨立 register 驅動。
  - RTC 已做薄骨架（韌體型，走 mailbox）。
- **需實機/大框架**（前述）：各引擎真硬體校驗、WiFi dot11/WDI、camera capture、GPU UMD、DOD modeset、PCIe demux、bthx.h。

## 全專案狀態
🟢 骨架 5 ｜ 🔵 邏輯完整 18 ｜ 🟡 部分 2 ｜ ✅ 完整 2 ｜ ➖ 免驅動 5。詳見 `RPi5-Porting-Status.md`。
