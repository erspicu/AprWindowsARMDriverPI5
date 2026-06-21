# B12：RP1 ADC / 溫度（bespoke KMDF）

> 教「**atomic 別名暫存器避免競態**」與「開機清 sticky error」的小章。

| | |
|---|---|
| **裝置** | RP1 SAR ADC（含溫度感測） |
| **Linux 源碼** | `drivers/hwmon/rp1-adc.c` |
| **Windows 框架** | bespoke KMDF |
| **本專案** | `windows_sources/adc/rp1-adc/` ｜ 狀態 🔵（sim 11/11） |

## 1. 為什麼用 atomic 別名而非 RMW？

CS 暫存器同時帶**即時狀態位**（READY/ERR）。如果用 read-modify-write 改 AINSEL（選通道），
讀到的 CS 可能含過期狀態位，寫回去就把它們蓋錯。源碼改用 **SET/CLR 別名視窗**：
```c
void AdcHwSelectChannel(void *Base, unsigned ch) {
    // 0x3000 CLR 別名：清 AINSEL 欄位
    WR32(Base, RP1_ADC_RWTYPE_CLR + RP1_ADC_CS, AINSEL_MASK << AINSEL_SHIFT);
    // 0x2000 SET 別名：設新通道
    WR32(Base, RP1_ADC_RWTYPE_SET + RP1_ADC_CS, (ch & AINSEL_MASK) << AINSEL_SHIFT);
}
```
> 和 [B3 GPIO 的 RIO 別名](B03-rp1-gpio.md) 是同一個硬體技巧——RP1 大量用這招避免 RMW 競態。

## 2. 開機清 sticky error（B 類）

```c
void AdcHwEnable(void *Base, int tempSensor) {
    WR32(Base, RP1_ADC_INTE, 0);                                  // 關中斷
    unsigned cs = RP1_ADC_CS_EN | RP1_ADC_CS_ERR_STICKY;          // ← 清 sticky error
    if (tempSensor) cs |= RP1_ADC_CS_TS_EN;
    WR32(Base, RP1_ADC_CS, cs);
}
```
> 漏了 `CS_ERR_STICKY`：開機若帶 sticky error，之後**每次轉換都被當成錯誤**。源碼 probe 有寫，移植要補。

## 3. one-shot 轉換

```c
int AdcHwConvert(void *Base, unsigned ch, unsigned *result) {
    AdcHwEnable(Base, 0);
    AdcHwSelectChannel(Base, ch);
    WR32(Base, RP1_ADC_RWTYPE_SET + RP1_ADC_CS, RP1_ADC_CS_START_ONCE);   // atomic 啟動
    /* 輪詢 CS 的 READY bit */
    *result = RD32(Base, RP1_ADC_RESULT) & 0xFFF;   // 12-bit
    return 0;
}
```

➡️ 回 [目錄](README.md)
