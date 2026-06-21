# B14：RP1 I2S ×3（DesignWare I2S，PortCls 音訊）

> 第一個 **PortCls/WaveRT** 音訊範例。也教「**漏設一個 CCR，BCLK 框不出字**」。

| | |
|---|---|
| **裝置** | RP1 DesignWare I2S（數位音訊） |
| **Linux 源碼** | `sound/soc/dwc/dwc-i2s.c` |
| **Windows 框架** | **PortCls / WaveRT** miniport |
| **本專案** | `windows_sources/audio/rp1-i2s/`（HAL）+ `rp1-i2s-portcls/`（WaveRT）｜ 狀態 🔵（sim 27/27） |

## 1. 音訊驅動的兩層

- **HAL（本章重點）**：DesignWare I2S 暫存器狀態機（config/start/stop/FIFO）。
- **PortCls/WaveRT glue**：DataRange、stream、DMA buffer——接 Windows 音訊堆疊（屬實機/大框架階段）。

## 2. 規格 / 暫存器重點

| 暫存器 | 用途 |
|--------|------|
| `CCR`(0x010) | **word-select size / sclk gate**（決定 BCLK 怎麼框字） |
| `TCR/RCR(ch)` | per-channel 解析度 |
| `TFCR/RFCR(ch)` | FIFO threshold |
| `TER/RER(ch)` | per-channel enable |
| `IMR(ch)` | per-channel 中斷遮罩 |
| `CER`/`IER` | clock enable / 全域 |

## 3. 經典案例：漏設 CCR（B 類）

Linux `dw_i2s_hw_params` 對每種格式同時設 **TCR 解析度** 與 **CCR**（word-select）。移植時只設了 TCR、
**漏了 CCR** → BCLK 不知道怎麼框字，word framing 未定。
```c
NTSTATUS Rp1I2sHwSetResolution(PRP1I2S_HW Hw, UINT32 Bits) {
    switch (Bits) {
    case 16: Hw->XferResolution = 0x02; Hw->Ccr = 0x00; break;   // ← 同時記 CCR
    case 24: Hw->XferResolution = 0x04; Hw->Ccr = 0x08; break;
    case 32: Hw->XferResolution = 0x05; Hw->Ccr = 0x10; break;
    }
    ...
}
// Config 結尾補寫 CCR（dw_i2s_hw_params: i2s_write_reg(CCR, ccr)）
Rp1I2sWrite(Hw, DW_I2S_CCR, Hw->Ccr);
```

另補：**IMR per-channel 中斷遮罩**（start 解遮罩、stop 遮罩）——源碼 `i2s_enable/disable_irqs` 有，移植漏抄。

## 4. 取樣率分頻在哪？

I2S 的 sample-rate 分頻**不在 DW I2S HAL 內**，而在 RP1 clock block：
audio PLL **61.44 MHz** ÷ 20 = 3.072 MHz = 48kHz×64。所以 48k 家族的取樣率由 [clk 章](B02-rp1-clocks.md) 那邊設。

➡️ 回 [目錄](README.md)
