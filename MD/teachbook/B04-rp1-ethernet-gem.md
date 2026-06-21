# B4：RP1 Gigabit Ethernet（Cadence GEM / macb，NDIS）

> 第一個 **NDIS** 範例（網路卡），也是本書講「A/B 缺漏」最經典的活教材——MDC 分頻同時是 A 類 + B 類。

| | |
|---|---|
| **裝置** | RP1 Gigabit Ethernet（Cadence GEM，Linux 叫 macb） |
| **Linux 源碼** | `drivers/net/ethernet/cadence/macb_main.c` |
| **Windows 框架** | **NDIS 6.x miniport** |
| **本專案** | `windows_sources/net/rp1-gem/` ｜ INF `rp1gem.inf` |
| **狀態** | 🔵 邏輯完整（x64 sim 24/24） |

## 1. 裝置概述

GEM 是個 **DMA descriptor-ring** 網卡：TX/RX 各有一圈描述子（descriptor），每個描述子指向一塊封包緩衝。
MAC 透過 **MDIO** 兩線去設定外接的 **PHY**（BCM54213）。

## 2. 規格 / 暫存器重點

| 暫存器 | offset | 用途 |
|--------|-------:|------|
| `NCR`（Network Control） | 0x00 | RXEN/TXEN/MPE(MDIO enable)/CLRSTAT |
| `NCFGR`（Network Config） | 0x04 | 速度/雙工 + **MDC 分頻(CLK 欄位)** + DRFCS(丟 FCS) + DBW |
| `NSR`/`TSR`/`RSR` | status | 狀態（W1C） |
| `MAN` | MDIO | 對 PHY 讀寫的命令 |
| `SA1B`/`SA1T` | MAC 位址 | |
| `RBQP`/`TBQP` | ring 基底 | RX/TX descriptor ring 實體位址 |
| `IDR`/`ISR` | 中斷 | |

## 3. 經典案例：MDC 分頻（A 類 + B 類同時中）

MDIO 的時脈 MDC 不能超過 **2.5 MHz**，所以要把 MAC 的 pclk 分頻。Linux：
```c
// macb_init_hw → gem_mdc_clk_div：依「實際 pclk」選分頻
unsigned long pclk = clk_get_rate(bp->pclk);
if (pclk <= 240000000) config = GEM_CLK_DIV96;   // (=5) 寫進 NCFGR[20:18]
config |= MACB_BIT(DRFCS);                         // 丟 RX FCS
macb_writel(bp, NCFGR, config);
```

**移植時踩的坑**：原本 `GemConfigure` 只寫了 `SPD|FD`，**漏抄 MDC 分頻與 DRFCS**（B 類）。
後果：MDC 用預設 pclk/8 = **25 MHz**，遠超 2.5 MHz → **PHY 完全讀不到**。

**怎麼補對**（A+B 三方對上）：
1. 量實機（A）：`clk_summary` → GEM pclk = clk_sys = **200 MHz** → 落在「≤240MHz → DIV96(=5)」。
2. devmem 驗：讀回 `NCFGR=0x01560048`，CLK 欄位[20:18]=5 ✓。
3. 補（B）：
```c
void GemConfigure(void *Base, int FullDuplex, int Speed100) {
    unsigned cfg = GEM_NCFGR_CLK_DIV96 | GEM_NCFGR_DRFCS;   // ← 補上 MDC 分頻 + 丟 FCS
    if (Speed100)   cfg |= GEM_NCFGR_SPD;
    if (FullDuplex) cfg |= GEM_NCFGR_FD;
    WR32(Base, GEM_NCFGR, cfg);
}
```

> 這就是「源碼有公式(B 依據) + 我漏抄(B) + 實機定 pclk(A) + devmem 驗」四件事對上才敢寫死的範例。
> 詳見 [第 08 章](08-hardware-truth-and-ab-gaps.md)。

## 4. descriptor ring + MDIO（code sample）

```c
// 組一個 MDIO 讀/寫命令（Clause 22）
unsigned GemMdioBuild(int phy, int reg, int isWrite, unsigned data) {
    return (1u<<30) | ((isWrite?1u:2u)<<28) | ((phy&0x1F)<<23) |
           ((reg&0x1F)<<18) | (2u<<16) | (data&0xFFFF);
}
// 初始化一個 RX descriptor：把 buffer 交給 HW（清 USED bit）
void GemRxInitDesc(GEM_DESC *d, unsigned bufPhys, int wrap) {
    d->addr = (bufPhys & GEM_RX_ADDR_MASK) | (wrap ? GEM_RX_WRAP : 0);
    d->ctrl = 0;
}
```

## 5. Windows 框架（NDIS miniport）

| NDIS callback | 對應 |
|---------------|------|
| `MiniportInitializeEx` | HAL reset、設 MAC、配 ring、enable RX/TX |
| `MiniportSendNetBufferLists` | 把封包填進 TX descriptor、清 USED 觸發送出 |
| `MiniportReturnNetBufferLists` | 回收 RX buffer |
| `MiniportInterrupt`/DPC | 讀 ISR、處理 TX done / RX 完成 |
| `MiniportOidRequest` | 回報/設定 OID（link state、MAC、統計） |

> 本專案 NDIS glue 為骨架；OID 完整處理、descriptor DMA 與真實收送屬實機階段。

## 6. 踩雷

- MDC 分頻**必設**（上面那段），否則 PHY 不通、整張網卡死。
- `GemReset` 易與 NDIS 的 `MINIPORT_RESET` handler 撞名 → HAL 函式取名 `GemHwReset`。
- ring buffer 的 cache coherency（ARM64）要用正確的 DMA buffer 屬性（實機階段）。

➡️ 回 [目錄](README.md)
