# WiFi 移植：策略全貌 + 開源參考資源

## 1. 策略：FullMAC 的捷徑

```
CYW43455（FullMAC）：韌體內含 802.11 MAC + WPA/WPA2 supplicant，對外吐 802.3 ethernet frame
   ↓ 所以
不必碰 Windows WLAN 協定堆疊（WiFiCx/WDI/Native-WiFi）
   ↓ 改成
WHD（晶片控制 library）→ 包成 NetAdapterCx「乙太網卡」→ 走 Windows SDIO
```

三個積木：
1. **晶片側**：Infineon **WHD**（Wi-Fi Host Driver，純 C、OS 無關）負責 DHD 協定 / SDIO 封包 / 韌體+NVRAM 載入。
2. **框架側**：**NetAdapterCx** 乙太網 miniport（Windows 以為插了實體網線）。
3. **匯流排側**：Windows 內建 **sdbus.sys**，自己寫 KMDF **SDIO function driver** 下 CMD52/CMD53。

> SSID/密碼透過 Registry 傳給驅動，啟動時叫晶片連 AP，連上後 802.3 封包進出 NetAdapterCx 的 Rx/Tx queue。

## 2. 開源參考資源 / SOURCES

### (A) 晶片 / 韌體側（最重要）

| 資源 | 用途 | 連結 |
|------|------|------|
| **Infineon WHD** ⭐ | 純 C、可移植的 CYW43455 FullMAC 驅動（自帶 DHD 協定/SDIO/韌體載入）。**核心 library** | https://github.com/Infineon/wifi-host-driver （建議 release-v3.x / master）|
| **Zephyr 的 WHD porting** ⭐⭐ | **最佳 porting 範本**：Zephyr 把 WHD 當 library 引入，`cybsp_wifi_os.c` + `cyhal_sdio_zephyr.c` 就是「非原生平台」的移植層——把 Zephyr API 換成 Windows kernel API 即可 | `zephyr/drivers/wifi/infineon/cyw43xxx/`（Zephyr source tree）|
| **Fuchsia brcmfmac（C++ 重寫）** | Google 用 C++ 重寫、**已解耦 cfg80211**，DHD 協定參考清晰 | Fuchsia source `src/connectivity/wlan/drivers/third_party/broadcom/brcmfmac/` |
| **FreeBSD `if_bwfm`** | 另一個乾淨的 Unix-like FullMAC 實作，可對照 | FreeBSD source `sys/dev/bwfm/` |
| **Firmware + NVRAM** | 一定要的 blob | `linux-firmware`：`cypress/cyfmac43455-sdio.bin` + Pi 的 nvram `.txt`；亦見 https://github.com/LibreELEC/brcmfmac_sdio-firmware-rpi |
| **nexmon** | Broadcom/Cypress 韌體 patching 框架，研究晶片/韌體行為用 | https://github.com/seemoo-lab/nexmon |

### (B) Windows 框架側

| 資源 | 用途 | 連結 |
|------|------|------|
| **NetAdapterCx 範例** | 偽裝乙太網卡的資料路徑（Rx/Tx queue） | `microsoft/Windows-driver-samples` → `network/netadaptercx`（如 `RxTxHandling`）|
| **NetAdapterCx 開源** | class extension 本體 | https://github.com/microsoft/Network-Adapter-Class-Extension |
| WiFiCx 文件 | （**本路線用不到**，但了解差異）| MS Learn: "Introduction to WiFiCx" |
| WDI 範例 | （**本路線用不到**）了解 Windows WiFi OID 狀態機 | `Windows-driver-samples` → `network/wlan/WDI` |
| SDIO（sdbus）| `SDBUS_INTERFACE_STANDARD`、`SdBusSubmitRequest` | WDK 標頭（`sdbus.h` 類）|

> **決策**：用 **NetAdapterCx**（現代）當乙太網卡，**不要** Native WiFi (dot11)（已強烈棄用、寫法繁瑣、Modern Standby 有問題）。
> WiFiCx 只有在「要完整 WiFi 管理功能（掃描列表/WPA3 offload/UI）」時才需要——本 MVP 不需要。

## 3. 兩條路線的工程量對比

| 路線 | 工程量 | 換到的功能 | 失去的 |
|------|--------|-----------|--------|
| **WHD + NetAdapterCx（本案）** | ~2-3 人月 | 能連指定 AP、能上網 | Windows WiFi UI / 掃描列表 / 動態切換 |
| 完整 WiFiCx 驅動 | 6-12 人月 | 完整 WiFi 體驗 | （除錯噩夢、WHQL）|

➡️ 實作細節見 [`02-implementation-guide.md`](02-implementation-guide.md)。
