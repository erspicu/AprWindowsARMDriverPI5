# WiFi（CYW43455）移植筆記

> Pi5 內建 WiFi（Broadcom/Cypress **CYW43455**，FullMAC，走 SDIO）移植到 **Windows on ARM64** 的研究與實作筆記。
> 來源：本專案分析 + Gemini 諮詢（紀錄於 `tools/knowledgebase/message/`）+ 開源資源查證。
> 建立：2026-06-21

## 一句話結論（路線定案）

> **放棄移植 Linux `brcmfmac/mac80211`；改用 Infineon WHD（純 C FullMAC library）+ 包成 NetAdapterCx
> 偽裝乙太網卡 + 透過 Windows 內建 SDIO（sdbus.sys / CMD52-53）。**
> 工程量從「完整 WiFiCx 6-12 人月」降到「**~2-3 人月**」，且有明確可落地步驟。代價：無 Windows WiFi 掃描 UI，
> SSID/密碼從 Registry 餵（dev board 可接受）。

## 為什麼不直接移植 brcmfmac？

802.11 協定層在 OS：Linux=cfg80211/mac80211、Windows=WiFiCx/WDI，**架構完全不同、無源碼可移植**；
brcmfmac 只是接 Linux stack 的薄橋。但 **CYW43455 是 FullMAC**——WPA supplicant 在晶片韌體內，晶片直接吐
**802.3 乙太網封包**。所以根本不必碰 Windows 的 WLAN 協定堆疊，當成一張「會自己連線的網卡」即可。

## 筆記索引

| 檔案 | 內容 |
|------|------|
| [`01-strategy-and-references.md`](01-strategy-and-references.md) | 策略全貌 + **所有開源參考資源 / SOURCES**（WHD、Zephyr porting 範本、Fuchsia、FreeBSD、firmware、MS 文件）|
| [`02-implementation-guide.md`](02-implementation-guide.md) | **可落地實作指南**：WHD 接進 kernel（cy_rtos/cyhal 對應）、SDIO function driver（CMD52/53）、OOB GPIO+ACPI、NetAdapterCx 資料路徑、分層圖、bring-up 順序 |
| [`03-pitfalls-and-open-questions.md`](03-pitfalls-and-open-questions.md) | **陷阱清單**（NVRAM 格式、DMA coherency、IRQL 邊界、電源）+ 待實機確認的未知數 + 備案 |

## 現況

- 本專案目前只有 NDIS 註冊骨架（`cyw43455wifi.sys`，🟡）——**與本筆記的新路線不同**，應改走 WHD+NetAdapterCx。
- 第一個里程碑（最該先做）：寫 SDIO function driver 空殼 → **用 CMD52 讀回 Chip ID `0x4345`** = 通了 80%。

## 務實備案（先求上網）

要 Pi5/Win11-ARM 立刻能上網，不必等 WiFi：**USB CDC-NCM/RNDIS 網卡**（inbox `cdce.sys`/`rndiscmp.sys`）
或 **Android 手機 USB 網路分享**（ARM64 即插即用）。
