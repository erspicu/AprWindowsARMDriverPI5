# A12：Watchdog / PM（bespoke KMDF）

> 小而完整的裝置，教「**password-protected 暫存器**」與「位元寬度/上限」的細節。

| | |
|---|---|
| **裝置** | BCM2712 PM watchdog |
| **Linux 源碼** | `drivers/watchdog/bcm2835_wdt.c` |
| **Windows 框架** | bespoke KMDF watchdog |
| **本專案** | `windows_sources/watchdog/bcm2712-wdt/` ｜ 狀態 🔵（sim 11/11） |

## 1. password-protected 暫存器

PM 區的暫存器寫入要帶 **password**（高位元組 `0x5A`），否則寫入被忽略：
```c
#define PM_PASSWORD       0x5A000000
#define PM_RSTC           0x1c
#define PM_WDOG           0x24
#define PM_WDOG_TIME_SET  0x000FFFFF   // 計數器只有 20-bit
#define PM_RSTC_WRCFG_FULL_RESET 0x20

void WdtHwStart(void *Base, unsigned secs) {
    unsigned ticks = (secs << 16) & PM_WDOG_TIME_SET;     // ticks = secs<<16
    WR32(Base, PM_WDOG, PM_PASSWORD | ticks);             // ← 一定要帶 password
    unsigned rstc = RD32(Base, PM_RSTC) & ~0x30;
    WR32(Base, PM_RSTC, PM_PASSWORD | rstc | PM_RSTC_WRCFG_FULL_RESET);
}
void WdtHwPing(void *Base, unsigned secs) {               // 餵狗 = 重設計數
    WR32(Base, PM_WDOG, PM_PASSWORD | ((secs<<16) & PM_WDOG_TIME_SET));
}
```

## 2. 位元寬度 / 上限（容易算錯）

- 計數器 **20-bit**：`PM_WDOG_TIME_SET = 0xFFFFF`，ticks = secs<<16 → **最大約 15 秒**（`0xFFFFF>>16`）。
  傳 20 秒會被截斷成奇怪的值——上限要 clamp。
- stop 的斷言遮罩：password 在**高位元組**，所以驗證 stop 時遮罩要用 `0x00FFFFFF`，不是 `0x0FFFFFFF`。

> 教學點：這章本身沒有 B 類漏抄，反而是「**模擬斷言一開始寫錯**」——把 20 秒當合法、遮罩多一個 0。
> 後來對齊「20-bit / 15 秒上限 / password 高位元組」才修對。提醒：**sim 的期望值也要算對**。

➡️ 回 [目錄](README.md)
