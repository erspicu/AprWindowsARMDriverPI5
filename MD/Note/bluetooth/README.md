# 藍牙（CYW43455 BT，UART HCI）移植筆記

> Pi5 內建藍牙（Cypress/Broadcom **CYW43455 combo 的 BT 部分，晶片近 BCM4345**，**走 UART + 標準 HCI/H4**）
> 移植到 **Windows on ARM64** 的研究與實作筆記。來源：本專案分析 + Gemini 5 輪諮詢（`tools/knowledgebase/message/`）。
> 建立：2026-06-22

## ⚠️ 路線更新（2026-06-25，覆蓋下方原始結論）

> **`bthx.h` 不在現代公開 WDK**（Win8 IHV-only，查證後確認缺）→ 自寫 bthx transport driver **不可行**。
> **改走 inbox `BthUart.sys`**：UART 下掛 ACPI（`brcm,bcm43438-bt` 類），INF `Needs=BthUart.NT`；
> 我們只做 **BCM `.hcd` Patch-RAM 載入 + baud 切換**的薄處理（已實作於 `windows_sources/bluetooth/bcm43438`，
> Phase A sim 35/35 + Phase B WDF glue /kernel 乾淨）。下方「bthx 路線」段落為歷史記錄、**已不採用**。

## ~~一句話結論（路線定案）~~ — 已被上方更新取代

> ~~寫一個 KMDF「Bluetooth Extensible Transport Driver」（`bthx.h` 介面），夾在 inbox `bthport.sys`（上）
> 與 UART（下）之間~~：只做 H4 byte-stream 搬運 + BCM vendor init（.hcd 韌體載入 + baud 切換）；
> L2CAP/SDP/配對/profiles 全由 `bthport.sys` 處理。

## 比 WiFi 簡單一個數量級

| | WiFi（CYW43455） | 藍牙（本案） |
|---|---|---|
| 上層協定 | 自寫 WiFiCx/WDI（或 WHD+偽裝 Ethernet） | **inbox `bthport` 全包**，你只做 transport |
| 你要寫的 | WHD 整合 + NetAdapterCx + SDIO | **H4 transport + BCM init**（~3000-5000 行）|
| 工程量 | ~2-3 人月 | **~1-1.5 人月**（熟 KMDF）/ 2-3（邊學）|
| SDK | — | ⚠️ **更正：`bthx.h` 不在現代公開 WDK**（Win8 IHV-only）→ 改走 inbox **BthUart.sys**（見頂部路線更新）|

> 為什麼簡單：HCI 是標準協定，Windows 有完整上層 stack。你的 driver 真的就是「**帶 vendor init 的 H4 byte-stream 搬運工**」。

## 筆記索引

| 檔案 | 內容 |
|------|------|
| [`01-strategy-and-references.md`](01-strategy-and-references.md) | 架構決策、難度對比、**開源參考 / SOURCES**（bthport 文件、Linux hci_bcm、BlueZ hciattach_bcm、.hcd 韌體）、bthx.h 位置 |
| [`02-implementation-guide.md`](02-implementation-guide.md) | **可落地實作指南**：transport core（bthx IOCTL + pended-read）、UART read pump + H4 狀態機表、BCM vendor init（opcode/.hcd/時序）、ACPI DSDT + GPIO/UART、init 期同步 HCI、INF/build |
| [`03-milestones-and-open-items.md`](03-milestones-and-open-items.md) | **bring-up 里程碑 M1-M5**（每步驗收點）+ 電源管理 + SCO 策略 + 待驗證/實機清單 |

## 第一個里程碑（先做這個）

> 寫一個**最小 WDF driver**：開 UART、拉高 BT_REG_ON、送 `HCI_Reset`(`0x01 0x03 0x0C 0x00`)、
> DbgPrint 收到的 bytes。**看到 Command Complete（`0x04 0x0E 0x04 0x01 0x03 0x0C 0x00`）= 晶片活、UART 通**——
> 剩下就是體力活（疊 BTHX、抄 H4 狀態機、處理 .hcd）。

## 現況
本專案目前有 H4 framing + BCM init 命令表雛形 + x64 sim（🔵）；接 bthport + ACPI + 實機載入屬 Pi5 階段。
