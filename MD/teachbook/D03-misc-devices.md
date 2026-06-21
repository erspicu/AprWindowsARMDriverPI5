# D3-D7：板載小裝置（PHY / 風扇 / 電源鍵 / LED / RTC / PMIC）

> 把幾個「小但有教學點」的板載裝置合在一章。

## D3：Ethernet PHY（BCM54213）
- **源碼**：`drivers/net/phy/broadcom.c`。
- **要點**：PHY 不是獨立驅動，是 [Ethernet（B4）](B04-rp1-ethernet-gem.md) 的一部分——MAC 透過 **MDIO** 設定它。
  所以 PHY「移植」其實是把 MDIO 讀寫 + PHY 初始化序列併進網卡 miniport。
- > 教學點：先確認 [GEM 的 MDC 分頻設對](B04-rp1-ethernet-gem.md)（否則 MDIO 不通，PHY 根本讀不到）。

## D4：散熱風扇（PWM-fan）/ 熱區
- **源碼**：`drivers/hwmon/pwm-fan.c`。
- **要點**：風扇接 [RP1 PWM1（B10）](B10-rp1-pwm.md)；轉速依溫度調。
- Windows：可做 KMDF + ACPI **熱區（thermal zone）**，讀溫度（[ADC/thermal](B12-rp1-adc.md)）→ 設 PWM duty。

## D5：電源鍵 / 狀態 LED
- **源碼**：`gpio_keys.c` / `leds-gpio.c`。
- **要點**：兩者都是 **GPIO 消費者**，不是獨立 register 驅動：
  - 電源鍵 = 某 GPIO pin 的中斷 → Windows **HID 按鈕**（ACPI 描述成 button）。
  - LED = 某 GPIO pin 輸出 → 簡單 KMDF 或 ACPI。
- > 教學點：**「消費者型」裝置**靠別的控制器（GPIO/HID）就能做，不必為它寫獨立 register HAL。

## D6：RTC（韌體型）
- **源碼**：`drivers/rtc/`（firmware）。**裝置**：`raspberrypi,rpi-rtc`。
- **要點**：Pi5 RTC **沒有 MMIO**，時間經 [VideoCore mailbox（A2）](A02-mailbox.md) 韌體 property 讀寫。
- 本專案：薄 KMDF 骨架（無 MMIO，走 mailbox #16），🟢。
- > 教學點：**不是每個裝置都有暫存器**——韌體型裝置的「HAL」其實是 mailbox 命令。

## D7：PMIC / regulators
- **源碼**：*無 DT 節點 / 無 Linux 驅動*。
- **要點**：Pi5 的 PMIC（DA9090 類）**由 VideoCore VPU 韌體管**（DT 只見 PMIC_INT/SCL/SDA 腳位）。
  Windows 端電源管理多走 **ACPI**，或透過韌體——通常**不需自寫 PMIC 驅動**。

➡️ 回 [目錄](README.md)
