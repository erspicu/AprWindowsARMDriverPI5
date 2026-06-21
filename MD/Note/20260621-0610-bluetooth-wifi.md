# 藍牙（BCM43438）+ WiFi（CYW43455）know-how

> 日期：2026-06-21 06:10

## 藍牙 — BCM43438 UART H4 HCI（KMDF 骨架）
- 產出 `windows_driver/bluetooth/btbcm.sys`（ARM64, KMDF）。
- **移植可沿用部分**（OS 無關）：
  - **H4 HCI 框架**：封包前綴 type（CMD=0x01/ACL=0x02/SCO=0x03/EVT=0x04），`BtBcmBuildCommand` 組 `[type][opcode LE16][plen][params]`。
  - **BCM43438 bring-up 序列**（port `bcm_setup`）：HCI_Reset → DownloadMinidriver(0xFC2E, 進 patchram) → WriteRAM(0xFC4C, 灌 .hcd) → LaunchRAM(0xFC4E) → SetUARTClock(0xFC45) → SetBaud(0xFC18) → SetBDADDR(0xFC01)。
- **卡住點（非硬體，是 SDK 缺檔）**：Windows BT **HCI transport 介面 `bthx.h` 不在此 WDK**（只有上緣 profile 的 `bthddi.h`）。故無法乾淨向 `bthport.sys` 註冊為 HCI transport。
  - 需 bthx.h（較舊 WDK 或 driver samples）才能接 bthport；或改走 vendor 專屬模型。
  - UART I/O 走 WDFIOTARGET 開 PL011（H4 over serial），需實機。

## WiFi — CYW43455 SDIO brcmfmac（NDIS 註冊骨架，🟡）
- 產出 `windows_driver/wifi/cyw43455wifi.sys`（ARM64, NDIS）。
- **僅 NDIS miniport 註冊**（reuse Ethernet 的 NDIS 模式）；`InterfaceType=NdisInterfaceInternal`（SDIO 內接）。
- **未完（工程量極大）**：Windows WiFi = **WDI（WLAN Device Driver Interface）** 或舊式 **Native-802.11** miniport —— scan/connect、數百個 dot11 OID、SDIO function driver（讀寫 CYW43455）、韌體載入。皆非無實機可完成。
- `windot11.h` 在 `shared`（dot11 型別）；WDI 需 `wdi.h` + IHV handler 海量實作。

## 結論
- 四項（網路/USB/藍牙/WiFi）已全數處理：網路=NDIS ✅、USB=inbox ➖、藍牙=KMDF H4 骨架 ✅（bthport 待 bthx.h）、WiFi=NDIS 註冊骨架 🟡（dot11/WDI 待大工程）。
