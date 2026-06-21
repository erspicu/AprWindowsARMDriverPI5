# A2：VideoCore 韌體 / Mailbox（bespoke KMDF）

> Tier 0 級別——很多裝置的 clock/power 由 **VideoCore 韌體**管，而與韌體溝通的管道就是 mailbox。

| | |
|---|---|
| **裝置** | BCM2712 VideoCore mailbox |
| **Linux 源碼** | `drivers/mailbox/bcm2835-mailbox.c` |
| **Windows 框架** | bespoke KMDF |
| **本專案** | `windows_sources/mailbox/bcm2712-mailbox/` ｜ 狀態 🔵（sim 8/8） |

## 1. 為什麼 mailbox 是 Tier 0

CPU 不直接控制很多周邊的時脈/電源；那是 **VideoCore VPU 韌體**在管。CPU 要「上電某裝置、設頻率、讀溫度」，
得**透過 mailbox 送 property 請求給韌體**。所以 mailbox 是「能不能讓其他裝置動起來」的前提。

## 2. mailbox 機制（極簡）

兩個方向各一個 FIFO，狀態暫存器標示 FULL/EMPTY：
```c
#define MAIL0_RD   0x00   // 讀（VC→ARM）
#define MAIL0_STA  0x18   // 狀態
#define MAIL1_WRT  0x20   // 寫（ARM→VC）
#define MAIL1_STA  0x38
#define ARM_MS_FULL  (1u<<31)
#define ARM_MS_EMPTY (1u<<30)

void MboxHwSend(void *Base, unsigned ch, unsigned data28) {
    while (RD32(Base, MAIL1_STA) & ARM_MS_FULL) ;          // 等不滿
    WR32(Base, MAIL1_WRT, (data28 & ~0xF) | (ch & 0xF));   // 低 4 bit = channel
}
unsigned MboxHwRecv(void *Base, unsigned ch) {
    for (;;) {
        while (RD32(Base, MAIL0_STA) & ARM_MS_EMPTY) ;     // 等有資料
        unsigned v = RD32(Base, MAIL0_RD);
        if ((v & 0xF) == ch) return v & ~0xF;              // channel 對才收
    }
}
```
> 訊息 = 28-bit 資料 + 低 4-bit channel。property channel（#8）配一塊對齊的 buffer 傳 tag。

## 3. 移植注意

- 本專案用**輪詢** send/recv 模型（簡單可靠）；若改中斷驅動，要設 `MAIL0_CNF` 的 IRQ enable（OS 介接）。
- RTC、部分 clock/power 都靠它 → 先把 mailbox 做穩，其他韌體型裝置才有依靠。

➡️ 回 [目錄](README.md)
