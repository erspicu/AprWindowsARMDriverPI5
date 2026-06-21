# B2：RP1 clocks（PLL / 時脈產生器，bespoke KMDF）

> Tier 0 級別的基礎裝置——很多 RP1 周邊要先有時脈才能動。也是「**read-modify-write 不能偷懶**」的好教材。

| | |
|---|---|
| **裝置** | RP1 時脈產生器（PLL_SYS/AUDIO/VIDEO + 各 clock gate） |
| **Linux 源碼** | `drivers/clk/clk-rp1.c` |
| **Windows 框架** | bespoke KMDF |
| **本專案** | `windows_sources/clk/rp1-clk/` ｜ 狀態 🔵（sim 12/12） |

## 1. PLL 設定流程

```c
int ClkHwPllConfig(void *Base, unsigned PllBase, unsigned FbDivInt,
                   unsigned FbDivFrac, unsigned Div1, unsigned Div2) {
    WR32(Base, PllBase + PLL_PWR, PLL_PWR_MASK);       // 先 power down
    WR32(Base, PllBase + PLL_FBDIV_INT,  FbDivInt);    // 設回授分頻
    WR32(Base, PllBase + PLL_FBDIV_FRAC, FbDivFrac);

    // ── B 類修正①：參考分頻 REFDIV 要設 1（RMW，保留唯讀 LOCK 等位元）──
    unsigned cs = RD32(Base, PllBase + PLL_CS);
    cs = (cs & ~PLL_CS_REFDIV_MASK) | PLL_CS_REFDIV_UNITY;
    WR32(Base, PllBase + PLL_CS, cs);

    WR32(Base, PllBase + PLL_PWR, FbDivFrac ? 0 : PLL_PWR_DSMPD);  // power up
    /* 輪詢 PLL_CS_LOCK(bit31) 直到鎖定 */

    // ── B 類修正②：post-divider 用 RMW，只動 DIV1/DIV2 ──
    unsigned prim = RD32(Base, PllBase + PLL_PRIM);
    prim &= ~(PLL_PRIM_DIV1_MASK | PLL_PRIM_DIV2_MASK);
    prim |= (Div1 << 16) | (Div2 << 12);
    WR32(Base, PllBase + PLL_PRIM, prim);
    return 0;
}
```

## 2. 兩個 B 類修正（為什麼 RMW 很重要）

| 漏的 | 後果 | 修法 |
|------|------|------|
| 沒設 `PLL_CS` 的 **REFDIV=1** | 殘留 REFDIV 把 VCO 參考分錯 → 頻率全錯/鎖不住 | 上電前 RMW 設 1 |
| `PLL_PRIM` **整條覆寫** | 把 DIV1/DIV2 以外的位元清成 0 | 改 **RMW** 只動兩個欄位 |

> 教學點：源碼 `rp1_pll_set_rate` 用的是 read-modify-write（只改該欄位）。移植時若圖方便「整條寫」，
> 就會清掉其他保留/控制位 → 偶發鎖不住。**RMW vs 覆寫**是移植常見陷阱。

## 3. Pi5 實測

- `clk_sys = 200 MHz`（feed RP1 I2C/SPI/SDIO 的分頻基準）、audio PLL=61.44MHz。
- devmem 讀 `PLL_SYS CS = 0x80000001` → bit31=LOCK ✓（驗 HAL 的 `PLL_CS_LOCK=BIT(31)`）。

## 4. clock gate

```c
void ClkHwEnable(void *Base, unsigned CtrlOff) {
    WR32(Base, CtrlOff, RD32(Base, CtrlOff) | CLK_CTRL_ENABLE);  // bit11
}
```

➡️ 回 [目錄](README.md)
