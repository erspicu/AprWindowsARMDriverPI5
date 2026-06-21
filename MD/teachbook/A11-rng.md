# A11：硬體亂數產生器 RNG（iproc-rng200，bespoke KMDF）

> 簡單裝置，但藏一個「**抄錯變體 + 漏暖機**」的雙重 B 類教訓。

| | |
|---|---|
| **裝置** | BCM2712 硬體 RNG（iproc-rng200，BCM2711 變體） |
| **Linux 源碼** | `drivers/char/hw_random/iproc-rng200.c`（`bcm2711_rng200_*` 路徑） |
| **Windows 框架** | bespoke KMDF |
| **本專案** | `windows_sources/rng/bcm2712-rng/` ｜ 狀態 🔵（sim 11/11） |

## 1. 陷阱：泛用 init vs BCM2711 init

源碼有泛用 `iproc_rng200_init`（只 enable RBGEN）與 **BCM2711 專屬 `bcm2711_rng200_init`**（三步）。
移植時若抄到泛用那套，開機初期會吐**低品質亂數**。BCM2711 三步：
```c
void RngHwInit(void *Base) {
    WR32(Base, RNG_INT_STATUS, 0xFFFFFFFF);
    if (RD32(Base, RNG_CTRL) & RNG_CTRL_RBGEN_MASK) return;        // 已啟用就跳過
    WR32(Base, RNG_TOTAL_BIT_COUNT_THRESHOLD, 0x40000);            // ① 丟早期低熵位
    WR32(Base, RNG_FIFO_COUNT, 2u << RNG_FIFO_THRESHOLD_SHIFT);    // ② FIFO 門檻
    WR32(Base, RNG_CTRL, (0x3u << RNG_CTRL_DIV_CTRL_SHIFT) | RNG_CTRL_RBGEN_MASK); // ③ 取樣率+啟用
}
```

## 2. 暖機等待（讀之前要等）

```c
int RngHwWaitWarmup(void *Base) {              // bcm2711_rng200_read 讀前必等
    for (unsigned i = 0; i < 100000; i++)
        if (RD32(Base, RNG_TOTAL_BIT_COUNT) > 16) return 1;   // 暖機完成
    return 0;
}
```
> 漏了暖機：開機後立即讀會拿到**未暖機**的資料。這是「源碼有寫(`while(... <16)`) 但移植漏抄」的 B 類。

## 3. 讀亂數

```c
unsigned RngHwFifoCount(void *Base){ return RD32(Base, RNG_FIFO_COUNT) & 0xFF; }
unsigned RngHwReadWord (void *Base){ return RD32(Base, RNG_FIFO_DATA); }
```

➡️ 回 [目錄](README.md)
