# WiFi 移植：陷阱清單 + 待實機確認 + 備案

## 1. 陷阱（會咬人，務必先知道）

| # | 陷阱 | 後果 | 對策 |
|---|------|------|------|
| 1 | **NVRAM 格式超嚴** | `.txt` 有多餘結尾空行 / 非 null-terminated key=value → 韌體**內部 crash 且無 log**，表現為 SDIO 失去回應 | 嚴格按格式解析成 null-terminated `key=value` array；載完 `.bin` 緊接著用 DHD 命令載 NVRAM |
| 2 | **DMA cache coherency（ARM64）** | CMD53 傳大封包若沒處理好 cache 維護 → **靜默記憶體損壞 / BugCheck 0x139 / 隨機藍白** | buffer 用 NonPaged；ARM64 的 cache flush/invalidate 要對；先小封包驗 |
| 3 | **IRQL 邊界** | 在 DISPATCH_LEVEL 呼叫 WHD blocking API → crash | WHD 一律 PASSIVE_LEVEL；NetAdapterCx ↔ WHD 之間隔 Ring Buffer + KEVENT |
| 4 | **SdBusSubmitRequest 是非同步** | WHD 預期同步返回 → 行為錯亂 | 自 alloc IRP + completion routine + `KeWaitForSingleObject` 轉同步 |
| 5 | **In-band 中斷不可靠** | sdbus 處理 in-band 延遲高、Broadcom 上易漏中斷 | **MVP 先 Polling**，資料通了再接 OOB GPIO；別死磕 in-band |
| 6 | **電源管理（休眠喚醒）** | 第一次實作休眠喚醒後 WiFi 必死 | MVP：`EvtDeviceD0Exit` 直接 power off 晶片、`D0Entry` 重走載韌體流程；別碰 FW keep-alive offload |
| 7 | **ACPI 沒描述子裝置** | OS 不會為 SDIO function 載入你的 .sys | DSDT 在 SDHCI 下掛子裝置設 Hardware ID（`SD\VID_02D0&PID_A9A6` 類）+ OOB GPIO 的 `_CRS`；ACPI 負責人要配合 |
| 8 | **記憶體必須 NonPaged** | SDIO DMA / 高 IRQL 存取 paged 記憶體 → crash | `ExAllocatePool2(POOL_FLAG_NON_PAGED, …)` |

## 2. 待實機確認（誠實標記的未知數）

- **SD host 能力協商**：`sdbus.sys` 是否在 Pi5 上自動切 1.8V + High-Speed(50MHz)？Pi5 的 Broadcom SD Host
  在 Windows 上「可能有雷」——要實機讀 `SDBUS_PROPERTY_BLOCK_SIZE` 等確認，並對齊 WHD buffer size。
- **WiFi 掛在哪個 SDIO host**：要先確認 CYW43455 在 Pi5 是掛在 BCM2712 的哪個 SDIO controller，且該 controller
  能被 Windows inbox `sdhc.sys`/對應 host 驅動認出（否則 sdbus 列舉不到晶片）。← **先決條件**
- **OOB GPIO 是哪根 pin**：WL_HOST_WAKE 對應的實際 GPIO（Pi5 DT 查），填進 ACPI `_CRS`。
- **WHD 版本/分支對 CYW43455**：確認 `Infineon/wifi-host-driver` 哪個 tag 支援 43455 + 對應韌體版本。
- **firmware 檔放哪**：Windows 上韌體 `.bin`/`.txt` 的放置路徑與載入方式（驅動自帶 / `%SystemRoot%`）。

## 3. 實作前的先決條件（依賴鏈）

```
能上網
 └─ NetAdapterCx 乙太網卡（你寫）
     └─ WHD 能跟韌體溝通（你 port）
         └─ SDIO function driver 能 CMD52 讀到 Chip ID 0x4345（你寫）← 第一里程碑
             └─ sdbus.sys 能列舉到 CYW43455
                 └─ Pi5 的 SDIO host controller 被 Windows 認出 ← 最底層先決
```
> 若最底層（SDIO host 在 Windows ARM64 被認出）卡住，整條都動不了——**建議先驗這層**。

## 4. 務實備案（先求上網，不等 WiFi）

| 方案 | 做法 | inbox 驅動 |
|------|------|-----------|
| USB 乙太網卡 | 買支援標準 **CDC-NCM / RNDIS** 的 USB 網卡 | `cdce.sys` / `rndiscmp.sys` |
| 手機 USB 分享 | Android 開 USB 網路分享 | ARM64 預設即插即用 |
| USB WiFi dongle | 挑 Windows ARM64 有 inbox/現成驅動的晶片 | 視晶片 |

## 5. 後續可再深追的點（若要更細）

目前的指南已到「可動手」等級；下列若實作時卡住可再諮詢 Gemini 或讀 Zephyr port：
- WHD porting 層的**確切函式清單**（讀 Zephyr `cyhal_sdio_zephyr.c` + `cybsp_wifi_os.c` 對照）。
- NetAdapterCx 的 **queue 建立**（`EvtAdapterCreateTxQueue/RxQueue`、`NET_RING` 配置）細節。
- 韌體**載入序列**的 DHD 命令細節（download mode、`.bin` 分塊、NVRAM、重啟）。
