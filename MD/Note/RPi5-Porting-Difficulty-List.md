# Raspberry Pi 5 → Windows on ARM 驅動「移植難易」清單

> 把裝置依「**好不好移植**」分類，方便挑工作。判別原則見下；逐裝置狀態見
> [`RPi5-Porting-Status.md`](RPi5-Porting-Status.md)，硬體全清單見 [`RPi5-Driver-Porting-Inventory.md`](RPi5-Driver-Porting-Inventory.md)。
> 建立：2026-06-21

## 判別原則（為什麼有的好移植、有的不行）

驅動 = **HAL（跟晶片講話的暫存器邏輯，可沿用 Linux）** + **OS 介接（接 Windows 框架，必須重寫、無源碼可抄）**。
**難易取決於這兩者的比例**：

| | HAL（可抄） | OS 介接（要重寫） | 結果 |
|---|---|---|---|
| **好移植** | 大頭、標準 IP | 小（薄框架）或可借 inbox | 源碼港得動 |
| **難移植** | 小頭 | **超大、平台專屬、無源碼**（整套協定堆疊） | 源碼幫不上主要工作 |

加分項（更好移植）：① 標準 IP（DesignWare/Cadence/PrimeCell，暫存器有公開文件）② Windows 有 inbox 驅動可借
③ 純軟體交付物（ACPI/INF）。減分項（更難）：① 龐大平台框架（WiFi 的 WDI/WiFiCx、GPU 的 WDDM）
② 需韌體 blob ③ 需缺的 SDK ④ 需實機才能驗的時序/DMA/中斷。

---

## ✅ 好移植（標準 IP + 薄框架，本專案多已到 🔵）

| 裝置 | 為什麼好 | Windows 框架 | 現況 |
|------|----------|--------------|------|
| RP1 I2C ×7 | DesignWare 標準 IP，HAL 是大頭 | SpbCx | 🔵 |
| RP1 SPI ×8 | DesignWare 標準 IP | SpbCx | 🔵 |
| BCM2712 I2C / SPI | BCM 自家但暫存器簡單 | SpbCx | 🔵 |
| RP1 / BCM2712 GPIO | 暫存器直接，GpioClx 框架明確 | GpioClx | 🔵 |
| RP1 PWM | 暫存器極簡 | bespoke KMDF | 🔵 |
| RP1 ADC | 簡單，atomic 別名 | bespoke KMDF | 🔵 |
| BCM2712 RNG | 簡單 FIFO | bespoke KMDF | 🔵 |
| RP1 clocks（PLL） | 暫存器直接 | bespoke KMDF | 🔵 |
| BCM2712 / RP1 DMA | descriptor 引擎（須選對變體） | bespoke KMDF | 🔵（BCM DMA40） |
| VideoCore / RP1 mailbox | 機制單純（FULL/EMPTY + FIFO） | bespoke KMDF | 🔵 |
| BCM2712 watchdog | 小而完整 | bespoke KMDF | 🔵 |
| RP1 GEM Ethernet | Cadence 標準 IP（HAL 可抄；NDIS glue 中等） | NDIS | 🔵（glue 待完整） |
| SDHCI（SD/eMMC） | SDHCI 標準 spec | SdPort | 🔵 |
| RP1 I2S | DesignWare 標準 IP（HAL 部分） | PortCls | 🔵（PortCls glue 大） |

## 🟡 中等 / 可借 inbox（少寫一點）

| 裝置 | 狀況 | 框架 |
|------|------|------|
| UART（PL011） | **借 inbox `SerPl011`（`ARMH0011`）**，多半不用自寫 | inbox |
| RP1 USB3（DWC3） | host 對外是 **xHCI** → 借 inbox `USBXHCI`；但要先確認 DWC3 core/PHY/模式已初始化 | inbox xHCI |
| GIC / timer / ECAM | **不寫驅動，寫 ACPI 表**（MADT/GTDT/MCFG） | ACPI（➖免驅動） |
| ACPI / INF（全部） | 純軟體交付物，無 .sys | ✅ 可完整完成 |
| RP1 PIO | 獨有，但暫存器操作不難（介面設計要想） | bespoke KMDF |
| RTC（韌體型） | 無 MMIO，走 mailbox property | bespoke KMDF |
| 電源鍵 / LED | GPIO/HID 消費者，靠別的控制器 | HID / KMDF |
| 風扇 | 接 PWM + ACPI 熱區 | KMDF + 熱區 |

---

## 🔴 難移植（OS 介接是整座山，源碼幫不上主要工作）— 逐一列原因

| 裝置 | **難在哪（具體原因）** | 缺的關鍵 | 框架 |
|------|------------------------|----------|------|
| **WiFi（CYW43455）** | ① 802.11 協定層在 OS（Linux=cfg80211/mac80211 vs Windows=WiFiCx/WDI，架構不同、無源碼可直接移植）② SDIO function driver ③ 韌體 blob 載入 ④ ARM64 無現成驅動。**但有捷徑**：CYW43455 是 FullMAC → 用 **Infineon WHD + 包成 NetAdapterCx 偽裝乙太網卡**，避開整個 WLAN stack，**~2-3 人月（非獨立專案級）**，有明確落地步驟 | WHD library + SDIO function driver（不必碰 WiFiCx）| NetAdapterCx（偽裝 Ethernet）|
| **V3D GPU** | GPU 驅動是「兩半」：**KMD（WDDM 排程/記憶體）+ UMD（把 D3D/OpenGL/Vulkan 編成 V3D 指令，等同移植 Mesa）**，兩半都巨大；Windows 無對應物 | WDDM KMD + UMD（Mesa 移植） | WDDM |
| **HDMI / HVS / 顯示管線** | 要完整 **WDDM**（modeset/present/電源…整套 DDI 合約），與 Linux DRM/KMS 架構不同；HDMI 不能單獨運作，要連 HVS+PixelValve 整條 | 完整 WDDM display miniport | WDDM |
| **相機 CSI / ISP（PiSP）** | **AVStream（KS filter/pin）+ 影像流水線**（CSI 收流、ISP 去馬賽克/降噪、DMA buffer 佇列、sensor I2C 控制）整套；ks.h 還要 C++ 編譯踩雷 | AVStream capture 完整實作 | AVStream / MFT |
| **HEVC 解碼** | 要實作 **DXVA / Media Foundation MFT**，與顯示/記憶體 surface 整合 | DXVA/MFT 解碼器 | MF / DXVA |
| **Bluetooth（BCM43438）** | ① 走 **bthport/BthMini**，需 `bthx.h` 等 **SDK 標頭（環境暫缺）** ② 廠商韌體 patch（.hcd）載入 ③ HCI 協定整合 | bthx SDK + bthport 整合 | bthport |
| **IOMMU** | DMA 位址轉換 + 保護，要接 Windows 的 **DMA remapping** 模型（高難度），且影響所有經它的 DMA 路徑 | DMA remapping 整合 | — |
| **DSI / DPI / VEC 顯示輸出** | 同 HDMI，要 WDDM（不同 connector/輸出級）；屬 Tier 3 | WDDM | WDDM |

### 難移植的共同特徵（一眼判斷）
- **OS 框架是一整套協定堆疊**（WiFiCx 的 802.11、WDDM 的顯示/GPU、AVStream 的影像）——這層平台專屬、**Linux 源碼裡沒有對應物可抄**。
- **需韌體 blob**（WiFi/BT）或**缺 SDK**（BT bthx.h）。
- **HAL 只佔一小塊**：就算把晶片暫存器邏輯抄完，也只完成了 ~10%。

---

## 一句話結論

> **好移植 = 標準 IP + 薄框架/可借 inbox（HAL 是大頭）；難移植 = 平台專屬的龐大協定堆疊（OS 介接是大頭、無源碼可抄）。**
> 有 Linux 源碼對前者幫助巨大（幾乎可沿用），對後者只幫到一小塊（晶片控制），真正的山還是要自己爬。

相關：**WiFi 的完整移植研究 + 可落地實作指南** → [`wifi/`](wifi/)（策略、開源資源、WHD/SDIO/NetAdapterCx 實作、陷阱）；
判別原則的完整版見教學書 [`../teachbook/02-linux-vs-windows-driver-model.md`](../teachbook/02-linux-vs-windows-driver-model.md)
與 [`../teachbook/08-hardware-truth-and-ab-gaps.md`](../teachbook/08-hardware-truth-and-ab-gaps.md)。
