# B15：RP1 類比音訊輸出（PortCls）

| | |
|---|---|
| **裝置** | RP1 類比音訊輸出（耳機/AV 孔） |
| **Linux 源碼** | `sound/soc/...`（rp1-audio-out） |
| **Windows 框架** | **PortCls** |
| **本專案** | 規劃中（🔲） |

## 1. 與 I2S 的關係

[I2S（B14）](B14-rp1-i2s.md) 是數位音訊介面；類比輸出是把數位訊號經 DAC/PWM 轉成類比聲音。
在 ALSA SoC 模型裡，這是一條 **machine + codec** 鏈：
```
CPU DAI (I2S/PWM) ── codec ── 類比輸出
```

## 2. 移植要點

- Windows 端走 **PortCls/WaveRT**（同 [I2S](B14-rp1-i2s.md) 的音訊框架）。
- 重點在 DataRange（支援的取樣率/格式）+ DMA + 與時脈（audio PLL 61.44MHz）的分頻。
- 屬 Tier 3 + 實機（真實 DMA、codec、混音）。

## 3. 注意

- `sound/soc/` 在本專案的 sparse checkout 需另補抓（DesignWare I2S 已補）。

➡️ 回 [目錄](README.md)
