# A3：BCM2712 clocks / power（韌體型，Tier 0）

> 教「**有些東西不是寫暫存器，而是請韌體做**」——BCM2712 的時脈/電源由 VideoCore 韌體管。

| | |
|---|---|
| **裝置** | BCM2712 韌體 clocks / power domains |
| **Linux 源碼** | `drivers/clk/bcm/clk-raspberrypi.c`、`drivers/pmdomain/bcm/raspberrypi-power.c` |
| **Windows 框架** | 併入韌體（mailbox）驅動 / ACPI 電源 |
| **本專案** | 規劃中（🔲），相依 [VC mailbox（A2）](A02-mailbox.md) |

## 1. 「韌體型」裝置的特性

和 [RP1 clocks（B2）](B02-rp1-clocks.md) 那種「直接寫 PLL 暫存器」不同，BCM2712 很多時脈/電源是
**透過 VideoCore 韌體 property channel** 設的：
```
CPU 想設某 clock 頻率 / 開某 power domain
   → 透過 VC mailbox 送 property tag（SET_CLOCK_RATE / SET_DOMAIN_STATE）
   → 韌體實際去調 PLL / 開電
```
所以這章其實是「**在韌體驅動上面包一層 clock/power API**」，沒有自己的 MMIO 暫存器。

## 2. 移植要點

- **先有 [A2 mailbox](A02-mailbox.md)**，這章才有依靠。
- clock：對應 property tag `GET/SET_CLOCK_RATE`、`GET_MAX_CLOCK_RATE`…
- power domain：`GET/SET_DOMAIN_STATE`。
- Windows 端：可做成 KMDF 服務供其他驅動查詢/設定，或部分走 ACPI 電源模型。

## 3. 教學點

> 移植時要先判斷「這個 clock 是直接寫暫存器，還是請韌體做」（看 Linux 驅動是 `clk-rp1`(暫存器) 還是
> `clk-raspberrypi`(韌體 mailbox)）。兩者移植策略完全不同——這也是「先讀懂源碼在跟誰講話」的重要性。

➡️ 回 [目錄](README.md)
