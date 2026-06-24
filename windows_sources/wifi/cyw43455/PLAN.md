# WiFi（CYW43455 SDIO FullMAC）實作規劃

> 路線見 [`MD/Note/wifi/`](../../../MD/Note/wifi/)。本檔是**逐步執行清單**。
> 圖例：☐ 待做 / ☑ 完成｜**[x64]** 本機 sim 可驗 → 🔵｜**[Pi5]** 需實機 → ✅。
> 現況：舊 **NDIS miniport 註冊骨架**（🟡）——**本規劃轉向新架構**（見下），舊 `miniport.c` 保留供參考/移除。

## ★ 架構轉向（重要）
放棄「NDIS/WDI 自寫 802.11」。改：**Infineon WHD（純 C FullMAC lib）+ NetAdapterCx 偽裝乙太網卡 + Windows SDIO（sdbus/CMD52-53）**。
CYW43455 是 FullMAC（韌體內含 supplicant，出 802.3 frame）→ 避開整個 WLAN stack。

```
NetAdapterCx(乙太網) ─ WHD core(C lib) ─ cyhal_sdio_win.c ─ sdbus.sys ─ CYW43455
                        cy_rtos_win.c(OS 抽象)
```

---

## Phase A — 純邏輯（x64 sim 可驗，推 🔵）

- ☑ **A1 [x64] SDIO 控制平面 + 讀 Chip ID 0x4345**（`sdio_core.c`）：**sim 過。**
  - `SDIO_OPS`（注入 Cmd52/Cmd53）；sim 用 **mock CYW43455 SDIO bus**。
  - backplane window：寫 F1 `0x1000A/B/C` 設高位址 → CMD53 讀 `0x8000|offset` → 取低 16 bit。
  - 已驗：chipid 0x4345 讀回、window LOW/MID/HIGH=00/00/18 且順序對、cmd52 錯誤傳遞。
  - ⚠️ SBADDR 確切 bit layout 待對 brcmfmac/實機（Pi5 C3）確認；目前邏輯自洽。
- ☑ **A2 [x64] NVRAM 預處理器**（`WhdNvramPreprocess`）：**sim 過。**
  - 去 `#`註解/空行、trim CRLF、每行 `\n`→`\0`、結尾補 `\0\0`。已驗：長度/分隔/雙 NUL/溢位防護。
- ☑ **A3 [x64] WHD resource 區塊餵給器**（`WhdResourceGetBlock`）：**sim 過。**
  - `offset=blockno*BLOCK`，回 `min(BLOCK, 剩餘)`，過尾回 0；指標指進 blob 不複製。
  - 驗：block0=512、block1=488(餘)、過尾=0、逐塊指標位移對、總和==Total。

> **2026-06-25 Pi5 實機驗證**（見 [`MD/Note/20260625-0030-wifi-bt-pi5-hardware-facts.md`](../../../MD/Note/20260625-0030-wifi-bt-pi5-hardware-facts.md)）：
> A1 chip-id @0x18000000=**0x15264345**（低16=0x4345）✓；A2 用真 nvram `brcmfmac43455-sdio.txt`(2074B→1743B) ✓。
- ☐ **A4 [x64] SDPCM/封包標頭邏輯**（若不直接靠 WHD）：H4 無關，這裡是 SDIO packet 的 add/remove front（給 whd_buffer 用）。*(視 WHD 整合程度，可能由 WHD 內建)*

## Phase B — WHD 整合 + WDF（需 WDK；部分 Pi5）

- ☐ **B1 取得 WHD source**：`Infineon/wifi-host-driver`（對 CYW43455 的 tag）；對照 Zephyr port（`cyabs_rtos_zephyr.c`/`cyhal_sdio.c`）。
- ☐ **B2 [x64-build] `cy_rtos_win.c`**：thread→`PsCreateSystemThread`、semaphore→`KeSemaphore`、mutex/timer/delay/malloc→kernel API（PASSIVE_LEVEL）。
- ☐ **B3 [x64-build] `cyhal_sdio_win.c`**：WHD SDIO HAL → `SDBUS_REQUEST_PACKET` + `SubmitRequest`（PASSIVE 同步）；接 A1 的 CMD52/53。
- ☐ **B4 build 整合**：WHD `.c` 直接編進 .sys（非 user-mode lib）；CRT stub（`snprintf`→`RtlStringCbVPrintfA`）；macro（`WHD_BUS_SDIO`/`CYW43455`/`WHD_CUSTOM_HAL`）；`__attribute__((packed))`→`#pragma pack`。
- ☐ **B5 [x64-build] SDIO function driver（KMDF）**：INF match `SD\VID_02D0&PID_xxxx`、取 `GUID_SDBUS_INTERFACE_STANDARD`。
- ☐ **B6 [x64-build] NetAdapterCx**：`NetDeviceInitConfig`→`NetAdapterCreate`→capabilities（MAC 用 `whd_wifi_get_mac_address`、TX/RX caps）→Tx/Rx queue（`EvtPacketQueueAdvance`，memcpy 與 WHD ring buffer 同步）。
- ☐ **B7 韌體嵌入**：`cyfmac43455-sdio.bin`/`.clm_blob` 編成 PE resource；NVRAM 用 A2 預處理。

## Phase C — 實機（Pi5）

- ☐ **C1 [Pi5] 先決**：確認 CYW43455 掛在 Pi5 哪個 SDIO host 且被 Windows sdbus 列舉到。← **最底層先決**
- ☐ **C2 [Pi5] ACPI DSDT**：SDIO 子裝置 HW ID + OOB GPIO（WL_HOST_WAKE）`_CRS`。
- ☐ **C3 [Pi5] M：讀 Chip ID 0x4345**（A1 接真 sdbus）= 通 80%。
- ☐ **C4 [Pi5] WHD on**：載韌體（polling mode 先）、`whd_wifi_on` 跟韌體溝通。
- ☐ **C5 [Pi5] NetAdapterCx 收送 + join AP**：SSID/密碼從 Registry（UTF-16→ASCII）；`whd_wifi_join` WPA2 → link up → ping 通。

---

## 下一步（本機可立刻做）
**A1 SDIO chip-id 讀取邏輯 + mock bus sim**，接著 A2 NVRAM 預處理、A3 resource 餵給器。
這些把 WiFi 控制平面邏輯推到 🔵（sim 過），等 Pi5 + WHD source 到位再接 B/C。
（B2/B3 需先取得 WHD source；在那之前先把不依賴 WHD 的 A 系列做完。）
