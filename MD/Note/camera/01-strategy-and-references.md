# 相機移植：策略 + 框架 + 參考

## 1. 硬體鏈與 Linux 對應

| Block | 做什麼 | 必驅動 | Linux |
|-------|--------|:---:|-------|
| **Sensor**（IMX708/OV5647）| 曝光 + 出 Bayer RAW | ✅ I2C 控 AE/AWB/fps/streaming | `drivers/media/i2c/imx708.c` |
| **RP1 CFE**（Camera Front End）| MIPI D-PHY + CSI-2 RX，解 MIPI packet，**PCIe DMA 把 RAW 寫 system RAM** | ✅ 設 D-PHY/lane/DMA 位址/frame-end IRQ | `drivers/media/platform/raspberrypi/rp1_cfe/` |
| **PiSP FE+BE**（BCM2712 硬體 ISP，M2M）| FE：RAW 初處理/壓縮；BE：demosaic/color matrix/gamma/lens-shading/downscale → YUV/RGB | ⛔ 可繞過（用軟體 ISP）| `drivers/media/platform/raspberrypi/pisp_be/` |

- 硬體只做 pixel 處理 + 產統計（histogram）；**libcamera 軟體跑 3A（IPA）**：拿統計算 exposure/gain/color matrix → I2C 下給 sensor、暫存器下給 PiSP，形成 control loop。

## 2. ISP 決策：繞過硬體 PiSP，走軟體 ISP

| 選項 | 評估 |
|------|------|
| (a) 完整移植硬體 PiSP（FE+BE）| 工程巨大；暫存器**未完全公開**；Linux 用 `pisp_be_config` 複雜結構寫進 RAM 讓 BE 讀；要重現 M2M 硬體佇列極難 debug |
| **(b) 軟體 ISP（DeviceMFT）★** | **最可行**：kernel CFE 只把 RAW 收進 RAM，**user-mode DeviceMFT** 做 debayer（CPU/GPU）→ NV12。先求有畫面 |
| (c) sensor 直出 YUV | IMX708/219/477 **無 sensor ISP**，只出 Bayer；少數低階（OV5640）能直出 YUV 但畫質差、非 Pi 生態 |

- **3A 重用**：libcamera 整包難 port 到 Windows，但其 **IPA（AE/AWB 純數學 C/C++）可抽出**放進 DeviceMFT 跑，算出的值經 KSPROPERTY 打回 kernel 寫 sensor。

## 3. Windows 框架
```
AVStream Capture Minidriver(.sys) → Windows Frame Server(user) → Media Foundation → App
                                    ↑ DeviceMFT(user-mode 軟體 ISP) 掛在 AVStream pin 之上
```
- 寫一個 **AVStream Capture Minidriver**：註冊 `KSCATEGORY_VIDEO_CAMERA`，KS filter + KS pin 協商 format。
- **ks.h C++ 坑**：`ks.h`/`ksmedia.h` 充斥 C/早期 COM 遺毒（`EXTERN_C` linkage、無名 struct、巨集衝突）→ 用 `extern "C"` 包，或把 KS descriptor/dispatch 寫成純 `.c`，硬體邏輯寫 `.cpp`，C-wrapper 串接。

## 4. 風險（最大未知數）
1. **PCIe DMA / IOMMU（WoA，生死線）**：RP1 在 PCIe，CFE DMA 寫 RAM 要過 SMMU。WoA HAL 對 BCM2712 PCIe + SMMU 成熟度未知；DMA 不合法 → 連 RAW 都收不到。做法見 [`02` §4](02-implementation.md)。
2. **ACPI / DT**：要改 UEFI/ACPI 讓 PnP 認出 RP1 下的 I2C bus + CFE block，否則 driver `DriverEntry` 都不觸發。
3. PiSP 暫存器未公開（若走硬體 ISP）。

## 5. 參考 SOURCES
| 資源 | 用途 |
|------|------|
| `Windows-driver-samples/avstream/avshws` | **假相機（test pattern）範本，必讀** |
| `Windows-driver-samples/avstream/sampledevicemft` | DeviceMFT 範本 |
| Linux `drivers/media/platform/raspberrypi/` + `drivers/media/i2c/imx708.c` | CFE/PiSP/sensor 暫存器與流程 |
| Raspberry Pi `libpisp`（user-space）| 看 PiSP 暫存器結構 |
| libcamera（IPA/3A）| 抽 AE/AWB 演算法 |
