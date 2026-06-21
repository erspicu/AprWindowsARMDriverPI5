# B13：RP1 PIO（Programmable I/O，bespoke KMDF）

> RP1 **獨有**的可程式 I/O——Windows 完全沒對應，需全新設計。教「**沒有現成框架時怎麼辦**」。

| | |
|---|---|
| **裝置** | RP1 PIO（可程式狀態機 I/O，類似 RP2040 的 PIO） |
| **Linux 源碼** | `drivers/misc/rp1-pio.c` |
| **Windows 框架** | bespoke KMDF（全自訂介面） |
| **本專案** | `windows_sources/pio/rp1-pio/` ｜ 狀態 🔵（sim 7/7） |

## 1. 什麼是 PIO

PIO 是幾組**小型可程式狀態機（SM）**，可用來「軟體定義」各種非標準的 I/O 協定（自訂時序的 bit-banging，
但由硬體 SM 跑、不吃 CPU）。每個 SM 有自己的指令記憶體、TX/RX FIFO、GPIO 對應。

## 2. 移植策略：沒有對應框架就「做資料管道」

Windows 沒有 PIO 這種東西，也沒有對應的 class 框架。移植策略是：
- HAL 提供 **SM 程式載入** + **FIFO push/pop** 的暫存器操作。
- bespoke KMDF 對外開**自訂 IOCTL**，讓 user-mode 載入 SM 程式、推資料、收資料。

```c
// FIFO 操作（每個 SM 一組）
void PioHwPushTx(void *Base, unsigned sm, unsigned word) {
    WR32(Base, PIO_FIFO_TX(sm), word);     // TX(sm) = 0x00 + sm*4
}
unsigned PioHwPopRx(void *Base, unsigned sm) {
    return RD32(Base, PIO_FIFO_RX(sm));    // RX(sm) = 0x10 + sm*4
}
```

> SM 程式本身（指令）多由韌體/user-mode 提供；HAL 只負責「把程式寫進 SM、搬資料進出 FIFO」。

## 3. 教學點

- **不是每個裝置都有現成框架**。遇到獨有硬體，回到最基本：用 bespoke KMDF 把「暫存器操作」包成
  user-mode 用得到的介面（IOCTL）。
- 設計介面時想清楚「誰提供 SM 程式、資料怎麼流」——這比暫存器本身難。

➡️ 回 [目錄](README.md)
