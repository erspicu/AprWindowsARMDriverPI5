# A9：BCM2712 SPI（SpbCx）

> 和 [RP1 DesignWare SPI（B8）](B08-rp1-designware-spi.md) 不同 IP——BCM 自家 SPI。教一個有趣的 **erratum**。

| | |
|---|---|
| **裝置** | BCM2712 SPI（bootloader EEPROM / 內部 SPI） |
| **Linux 源碼** | `drivers/spi/spi-bcm2835.c` |
| **Windows 框架** | **SpbCx** |
| **本專案** | `windows_sources/spi/bcm2712-spi/` ｜ INF `bcm2712spi.inf` ｜ 狀態 🔵（sim 18/18） |

## 1. 暫存器（極簡）

| 暫存器 | 用途 |
|--------|------|
| `CS`(0x00) | 控制 + 狀態：TA(啟動)、CPOL/CPHA、CLEAR_RX/TX、CS 選擇、DONE/TXD/RXD |
| `FIFO`(0x04) | 資料 |
| `CLK`(0x08) | CDIV 分頻：SCLK = core / CDIV |
| `DLEN`(0x0c) | DMA 長度 |

core clock = **vpu-clock 750 MHz**（Pi5 實測）。

## 2. 經典案例：收尾要寫 DONE（erratum，B 類）

`bcm2835_spi_reset_hw` 註解明言：「**不在每次傳輸結尾寫 DONE，傳輸有時會停擺**」。移植時收尾只寫 `CS=0`，
漏了 DONE + 清 FIFO：
```c
// 收尾：TA 清掉 + 寫 DONE(erratum) + 清雙 FIFO + DLEN=0
#define BCM_SPI_CS_RESET (BCM_SPI_CS_DONE | BCM_SPI_CS_CLEAR_RX | BCM_SPI_CS_CLEAR_TX)
BcmSpiWrite(Hw, BCM_SPI_CS, BCM_SPI_CS_RESET);
BcmSpiWrite(Hw, BCM_SPI_DLEN, 0);
```
> 教學點：**硬體 erratum / workaround 也是「源碼有寫」的 B 類**——別把它當成多餘程式刪掉。

## 3. 另兩個眉角

- **CDIV=0 是合法值**（最慢，÷65536）：別把 0 硬改成預設值（呼叫端傳明確預設）。
- **native CS 無效化**：起手 CS 設 `CS_01|CS_10`(=3) 讓 native CE 腳無效，避免 HW 自驅 CE 干擾 GPIO-CS 匯流排。

```c
UINT16 BcmSpiHwClkDiv(UINT32 SclkHz) {            // core = 750 MHz（Pi5 實測）
    UINT32 cdiv = (750000000u + SclkHz - 1) / SclkHz;  // ceil
    if (cdiv & 1) cdiv++;                              // CDIV 須偶數
    if (cdiv >= 65536) return 0;                       // 0 == 65536（最慢）
    return (UINT16)cdiv;
}
```

➡️ 回 [目錄](README.md)
