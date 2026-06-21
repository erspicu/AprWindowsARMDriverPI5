# B11：RP1 SD/MMC（DesignWare MSHC，SdPort）

> 和 [BCM2712 的 SDHCI（A5）](A05-sdhci-sdport.md) 是不同控制器，但都走 **SdPort**——教「SDHCI 標準的好處」。

| | |
|---|---|
| **裝置** | RP1 SD/MMC ×2（DesignWare Mobile Storage Host Controller / dwcmshc） |
| **Linux 源碼** | `drivers/mmc/host/sdhci-of-dwcmshc.c` |
| **Windows 框架** | **SdPort** miniport |
| **本專案** | 規劃中（🔲）；注意 Pi5 主 SD/eMMC 其實在 [BCM2712（A5）](A05-sdhci-sdport.md) |

## 1. 都是 SDHCI 標準 → 大量共用

dwcmshc 與 brcmstb 都是 **SDHCI spec 相容**控制器，標準暫存器（COMMAND/TRANSFER_MODE/INT_STATUS…）
一樣——所以 [A5 的 SDHCI HAL](A05-sdhci-sdport.md) 可大量共用，只差**vendor 特定暫存器**：
- dwcmshc 有自己的 reset 後處理（reset CMD 後要清 INT_RESPONSE 的 quirk）。
- 各家 PHY/tuning 暫存器在獨立的 cfg 區塊。

> 教學點：**SDHCI 是標準** → 移植一顆，其他顆只補 vendor quirk。這就是「標準 IP 重用機會高」的好處
> （見 [第 01 章](01-pi5-architecture.md) §1.6）。

## 2. 歸屬提醒

Pi5 的**主要** SD 卡 + eMMC 在 BCM2712（A5），不是 RP1。RP1 的 SDIO 在 Pi5 此 config 未啟用
（本專案 Pi5 實測 `/sys/class/mmc_host` 兩顆都在 BCM2712）。RP1 SDIO 多用於 WiFi 模組等。

## 3. 移植要點

- 共用 A5 的 SDHCI 命令引擎 HAL（reset/clock/power/sendcommand/int）。
- 補 dwcmshc 的 vendor reset quirk（`dwcmshc_reset`）。
- base clock / PHY tuning 用實機量。

➡️ 回 [目錄](README.md)
