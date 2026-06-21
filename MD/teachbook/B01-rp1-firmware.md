# B1：RP1 韌體 / mailbox（Tier 0）

> 和 [VideoCore mailbox（A2）](A02-mailbox.md) 概念類似，但這是 **RP1 自己的韌體**——gating RP1 子系統的部分設定。

| | |
|---|---|
| **裝置** | RP1 韌體介面 / mailbox |
| **Linux 源碼** | `drivers/firmware/rp1-fw.c`、`drivers/mailbox/rp1-mailbox.c` |
| **Windows 框架** | bespoke KMDF |
| **本專案** | 規劃中（🔲） |

## 1. 為什麼要它

RP1 內部有些設定（部分 clock/power、腳位）由 **RP1 自己的韌體**管，CPU 透過 RP1 mailbox 下請求。
和 BCM2712 的 VideoCore 韌體是**兩套不同的韌體**（別混）：

| | VideoCore 韌體（A2） | RP1 韌體（本章） |
|---|---|---|
| 跑在 | BCM2712 的 VPU | RP1 內 |
| 管 | BCM 側 clock/power/PMIC/溫度 | RP1 側部分設定 |
| 管道 | BCM mailbox `0x7c013880` | RP1 mailbox（BAR1 內 `0x8000`） |

## 2. 移植要點

- mailbox 機制與 A2 雷同（FULL/EMPTY 狀態 + 讀寫 FIFO + property tag）。
- 屬 **Tier 0**：某些 RP1 周邊要先透過它上電/設定，所以要早做。
- 本專案目前 RP1 多數周邊直接操作 BAR1 暫存器（HAL），韌體相依的部分待補。

## 3. 與 clocks 的關係

RP1 clocks（[B2](B02-rp1-clocks.md)）部分透過 PLL 直接設、部分可能經韌體；移植時要釐清「哪些是直接寫暫存器、哪些要走 mailbox」。

➡️ 回 [目錄](README.md)
