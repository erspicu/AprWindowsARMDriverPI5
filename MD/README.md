# MD — 文件

移植過程的文件，分三類子目錄：

| 子目錄 | 用途 |
|--------|------|
| [`Note/`](Note/) | 各階段 **know-how 筆記**（檔名 `年月日-時間-主題.md`）+ 進度/清單核心文件。 |
| [`Skill/`](Skill/) | **操作型 how-to**（建置工具鏈、Pi5 SSH 硬體萃取…），CLAUDE.md 細節引導至此。 |
| [`teachbook/`](teachbook/) | **教學書 × Pi5 硬體規格書**：Windows 驅動移植教學（基礎觀念 + 逐一硬體規格與移植，附 code sample）。持續撰寫中。 |
| [`History/`](History/) | **開發歷程**（已完成/解除的自動移植 loop 紀錄）。 |

## Skill（操作 how-to）

| 檔案 | 內容 |
|------|------|
| [`Skill/build-toolchain.md`](Skill/build-toolchain.md) | x64→ARM64 交叉編譯、cl/link 配方、x64 sim、ASL/INF。 |
| [`Skill/pi5-ssh-hardware-facts.md`](Skill/pi5-ssh-hardware-facts.md) | Pi5 SSH 連線法、唯讀安全規則、規格缺漏 A/B 判別、已萃取硬體事實。 |

## 核心文件（單一真相來源）

| 檔案 | 內容 |
|------|------|
| [`HANDOFF.md`](HANDOFF.md) | **接手/全專案總說明**（先讀這個）：雙機模型、倉庫結構、關鍵決策、慣例、UEFI/GPU 線、16GB 問題、bring-up 第一步、新 session 從這裡開始。|
| [`Note/RPi5-Porting-Status.md`](Note/RPi5-Porting-Status.md) | **全 Pi5 驅動移植狀態清單**（狀態階梯 ⬜🟡🟢🔵✅➖、雙機交接盤點、各驅動產出路徑/餘下項目）。每完成階段性產出即更新。|
| [`Note/RPi5-Driver-Porting-Inventory.md`](Note/RPi5-Driver-Porting-Inventory.md) | 硬體**全裝置清單**與 Windows 驅動移植對照表。|
| [`Note/RPi5-Porting-Difficulty-List.md`](Note/RPi5-Porting-Difficulty-List.md) | **移植難易清單**：好移植 vs 難移植（如 WiFi）分類 + 逐一原因。|
| [`Note/wifi/`](Note/wifi/) | **WiFi（CYW43455）移植專題**：WHD + NetAdapterCx 路線、開源資源、可落地實作指南、陷阱。|
| [`Note/bluetooth/`](Note/bluetooth/) | **藍牙（CYW43455 BT）移植專題**：**inbox BthUart.sys 路線**（`bthx.h` 不在現代 WDK）、BCM .hcd 韌體、ACPI、bring-up 里程碑。|
| [`Note/gpu/`](Note/gpu/) | **V3D GPU（VideoCore VII）移植專題（早期研究）**：WDDM 架構、Mesa+DXVK 路線、知識邊界。**最新進度/策略見下方 `20260626-02xx/03xx` 系列**（KMD+UMD 已實作骨架）。|
| [`Note/hevc/`](Note/hevc/) | **HEVC 硬解（rpivid）移植專題**：Pi5 codec 涵蓋真相（HEVC only，非 YouTube）、KMDF + 獨立 MFT 路線、stateless 解碼/SAND、整合現實。|
| [`Note/camera/`](Note/camera/) | **相機（CSI-2/PiSP）移植專題**：Sensor→RP1 CFE→ISP 鏈、AVStream + DeviceMFT 軟體 ISP 路線、WoA PCIe DMA/IOMMU 關鍵、5 步里程碑。|
| [`Note/iommu/`](Note/iommu/) | **IOMMU/SMMU 移植專題**：釐清「不寫驅動，是寫 ACPI IORT」、BCM2712 MMU-500、IORT 三 node、所有 PCIe DMA 的先決條件。|
| [`Note/display-outputs/`](Note/display-outputs/) | **多顯示輸出專題**：HDMI×2/DSI/VEC/DPI 在 WDDM 的多 head(VidPN)、DSI 面板初始化、觸控 I2C HID、優先序（HDMI1 最划算）。|
| [`Note/hdmi-audio/`](Note/hdmi-audio/) | **HDMI 音訊專題**：PortCls/WaveRT miniport、MAI/ACR、與 DOD 的 private interface 耦合、「嗶一聲」里程碑。|

## 重點 know-how 筆記

| 檔案 | 主題 |
|------|------|
| [`Note/20260621-0720-pi5-linux-hardware-facts.md`](Note/20260621-0720-pi5-linux-hardware-facts.md) | Pi5 實機（Linux）SSH 唯讀萃取的**硬體事實**（BAR/偏移/IRQ/時脈）+ 「規格缺漏 A/B 兩類」判別原則。|
| [`Note/20260621-0820-source-vs-port-bclass-gaps.md`](Note/20260621-0820-source-vs-port-bclass-gaps.md) | 「Linux 源碼 vs 移植」逐項比對，14 個 HAL 的 **B 類缺漏**補正紀錄。|
| `Note/20260621-*-*.md` | 各階段（ACPI/SDHCI/PCIe/bespoke KMDF/sim 等）的逐步紀錄。|
| **UEFI 韌體線** | |
| [`Skill/pi5-uefi-build.md`](Skill/pi5-uefi-build.md) | **✅ 已驗證的 Pi5 UEFI build 配方 + 所有踩坑**（worproject fork、subhook 鏡像、`-t GCC`、`_HID` hex…）。|
| [`Note/20260625-2230-pi5-uefi-acpi-integration.md`](Note/20260625-2230-pi5-uefi-acpi-integration.md) | UEFI ACPI 整合策略 + **決策：做法 A（客製 UEFI ACPI）為主線、rp1bus 為備案**。|
| [`Note/20260626-0130-win11arm-driver-packaging-sop.md`](Note/20260626-0130-win11arm-driver-packaging-sop.md) | 封裝 SOP：ACPI 進 UEFI + 驅動 DISM 注入 Windows 映像（驅動**不**進 UEFI）。|
| **GPU 線（KMD+UMD 實作）** | |
| [`Note/20260626-0200-pi5-gpu-accel-stack.md`](Note/20260626-0200-pi5-gpu-accel-stack.md) | Pi5 GPU 加速堆疊 + **Vulkan-first 策略**（KMD + v3dv → Zink/DXVK/vkd3d/clvk）。|
| [`Note/20260626-0230-wddm-kmd-references.md`](Note/20260626-0230-wddm-kmd-references.md) | WDDM KMD 架構 + 參考源（KMDOD/viogpu/Mesa，`gpu_driver_sources/`）。|
| [`Note/20260626-0300-v3dv-umd-winsys-port-surface.md`](Note/20260626-0300-v3dv-umd-winsys-port-surface.md) | v3dv UMD 的 DRM ioctl → D3DKMT 翻譯面分析。|
| [`Note/20260626-0330-pi5-v3d-hardware-facts.md`](Note/20260626-0330-pi5-v3d-hardware-facts.md) | Pi5 實機 V3D 真相（IDENT/MMU_CTL/中斷/vulkaninfo）校正 KMD/UMD。|

> 筆記檔名慣例：`年月日-時間-主題.md`（例 `20260621-0720-pi5-linux-hardware-facts.md`）。
