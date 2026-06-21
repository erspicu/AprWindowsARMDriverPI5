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
| **V3D GPU** | 完整原生 D3D WDDM 極大（KMD+UMD+DXIL→QPU 編譯器＝數十人×年，小團隊不可行）。**但有務實玩家路線**：V3D 是**最開源的 GPU**（Mesa `v3d`/`v3dv` 就是 Broadcom 官方驅動，暫存器/CL/QPU/MMU 100% 公開）→ 精簡 WDDM KMD + 移植 Mesa(GL/Vulkan ICD) + **DXVK/VKD3D**，~1 人年給玩家 3D 加速；先做 DOD+WARP 可用桌面。詳見 [`gpu/`](gpu/) | DOD（桌面）/ KMD+Mesa+DXVK（加速）| WDDM / Vulkan ICD |
| **HDMI / HVS / 顯示管線** | 點亮畫面用 **DOD**（不必完整 render WDDM）：Hybrid（mailbox modeset + MMIO flip）+ UEFI GOP 劫持最快看到桌面。完整 modeset 走 HVS+PixelValve+HDMI。詳見 [`gpu/02-dod-implementation.md`](gpu/02-dod-implementation.md) + [`display-outputs/`](display-outputs/) | DOD（點亮）/ WDDM（加速）| WDDM |
| **相機 CSI / ISP（PiSP）** | 整套 **AVStream + 影像流水線**（CSI 收流、debayer、DMA 佇列、sensor I2C、ks.h C++ 坑）。**務實路線**：繞過硬體 PiSP → kernel CFE 收 Bayer RAW + **user-mode DeviceMFT 軟體 debayer**。成敗在 **WoA PCIe DMA/IOMMU**（用 WDF common buffer 的 IOVA）。**~6-9 人月**，5 步里程碑。詳見 [`camera/`](camera/) | AVStream miniport + DeviceMFT | AVStream / MFT |
| **HEVC 解碼（rpivid）** | Pi5 **只有 HEVC 硬解**（無 H.264/VP9/AV1 → **YouTube 4K 用不到**，只能本地 4K HEVC）。走**獨立 Sync MFT + KMDF**（繞過沒做的 WDDM）；`rpivid` 是 stateless（host 自 parse + 管 DPB）+ 輸出 **SAND** 要 NEON 轉 NV12。**~3.5-4.5 人月**。只惠及電影與電視/WMP（Edge/VLC 走 DXVA 不吃 MFT）。詳見 [`hevc/`](hevc/) | KMDF + user-mode MFT | Media Foundation（MFT）|
| **Bluetooth（CYW43455 BT）** | **其實比想像簡單（比 WiFi 低一個數量級）**：HCI 是標準協定、inbox **`bthport.sys` 全包上層**；你只寫一個 **Bluetooth Extensible Transport Driver**（H4 byte-stream 搬運 + BCM `.hcd` 韌體載入 + baud 切換）。`bthx.h` **就在標準 WDK**（先前「缺 SDK」不成立）。**~1-1.5 人月**，有明確 5 步 bring-up。詳見 [`bluetooth/`](bluetooth/) | KMDF transport driver（接 inbox bthport）| bthport（BTHX）|
| **IOMMU / SMMU** | **其實不寫驅動**：Windows ARM64 內建 SMMUv2 支援。「移植」＝**把 BCM2712 MMU-500(SMMUv2) 寫進 ACPI IORT**（UEFI/EDK2），HAL 自動建 DMA 映射；driver 只用 WDF DMA。難在 debug ACPI + 是**所有 PCIe/RP1 DMA 的先決條件**。詳見 [`iommu/`](iommu/) | ACPI IORT（非 .sys）| HAL 內建 |
| **多顯示輸出（HDMI×2/DSI/VEC/DPI）** | 都騎同套 DOD/WDDM，靠 **VidPN** 表達多 head。**HDMI1 最划算**（+10-20%，雙螢幕）；DSI niche（+100-150%，要 MIPI DCS 面板初始化）；VEC 極 niche（+40%）；**DPI Pi5 已淘汰**（GPIO 在 RP1）。觸控走獨立 I2C HID(PNP0C50)。詳見 [`display-outputs/`](display-outputs/) | DOD 多 head + DSI 面板序列 | WDDM |

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
