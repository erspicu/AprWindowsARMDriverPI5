# D1：WiFi（CYW43455，NDIS 802.11）

> 與 [Bluetooth（D2）](D02-bluetooth.md) 同模組，但 WiFi 走 **SDIO**、stack 龐大——本書標示**最高難度之一**。

| | |
|---|---|
| **裝置** | Cypress/Broadcom CYW43455 WiFi（走 SDIO2） |
| **Linux 源碼** | `drivers/net/wireless/broadcom/brcm80211/brcmfmac/` |
| **Windows 框架** | **NDIS 原生 802.11（Native WiFi / WDI）** |
| **本專案** | NDIS 註冊骨架（`cyw43455wifi.sys`，缺 dot11/WDI 協定層），🟡 |

## 1. 為什麼難

WiFi 不只是「一張網卡」，而是一整套：
- **SDIO function driver**：晶片掛在 SDIO 上，要先有 SDIO 匯流排存取。
- **韌體載入**：CYW43455 開機要載 .bin 韌體 + nvram（同 BT 的韌體載入概念）。
- **802.11 協定層**：scan / 認證 / 連線 / 金鑰 / 漫遊…—— Windows 要 **Native WiFi（dot11）或 WDI** miniport，
  這層**極大**。

## 2. 移植層次

```
NDIS WiFi miniport (dot11/WDI)        ← 協定層，最大工程
   └─ brcmfmac 等價邏輯（命令/事件、韌體介面）
        └─ SDIO function driver（在 SDIO 上讀寫晶片）
             └─ SDIO host（[SD/MMC](A05-sdhci-sdport.md) 類）
```

## 3. 現況

NDIS 註冊骨架已 link；SDIO function、韌體載入、dot11/WDI 協定層**工程量極大**，屬 Tier 4 + 實機。
> 教學點：無線裝置常是「網卡 + 韌體 + 龐大協定 stack」三件套，**評估投入前先想清楚 ROI**。

➡️ 回 [目錄](README.md)
