# 相機移植：可落地實作

> 承 [`01`](01-strategy-and-references.md)。rp1-cfe 暫存器 offset 一律標「需查 Linux rp1-cfe source」。

## 1. Sensor + CSI-2 + RP1 CFE bring-up

### Sensor（IMX708）I2C
- I2C 走已移植的 **`rp1-dw-i2c`**（ACPI 宣告 camera 節點相依 RP1 I2C）。
- 初始化序列直接抽 Linux `imx708.c` 的 `imx708_mode_common_regs` + 對應 resolution register array。
- 關鍵暫存器：`0x0100` mode select（standby/streaming）、`0x0114` CSI lane mode（2/4 lane）、`0x0202` integration time、`0x0204` analog gain、`0x0000` chip id（讀回 `0x0708`）。

### CSI-2 / CFE（暫存器需查 rp1-cfe source）
- `CFE_CSI2_CTRL`：lane count、enable CSI-2 RX。
- `CFE_CH0_CTRL`：data type（RAW10=`0x2B`）。
- `CFE_CH0_DMA_BASE`：**填 IOVA**（見 §4）。
- `CFE_CH0_DMA_STRIDE` / `FRM_SIZE`：每行 byte、總大小。
- `CFE_IRQ_EN`：開 frame-end 中斷。
- CFE 是 bus-master DMA，把整張 RAW 連續寫進指定 buffer（packed RAW10：5 bytes/4 pixels）。

### 時脈/電源
- EXTCLK（24MHz）由 RP1 `clk_cam` 出（查 `clk-rp1.c`）；XSHUTDN（reset/powerdown）走 RP1 GPIO，拉高才能 I2C。

## 2. AVStream capture miniport

- 註冊：`KsInitializeDriver` + `KSCATEGORY_VIDEO_CAMERA`/`KSCATEGORY_CAPTURE`；定義 capture pin。
- **format**：輸出 RAW，Windows 不認 MIPI RAW10 → 定義自訂 subtype GUID（`MEDIASUBTYPE_RP1_RAW10`），只在 kernel↔DeviceMFT 間流通。
- **buffer 流（DMA→AVStream）**：
  1. 預配 WDF common buffer，物理(IOVA)位址寫 CFE `DMA_BASE`。
  2. CFE 寫完一張 → MSI-X 中斷 → ISR → DPC。
  3. DPC：`KsPinGetLeadingEdgeStreamPointer` 取空 buffer → `RtlCopyMemory` 把 common buffer 複製進 `KSSTREAM_POINTER`（第一版先 copy 保證能動；進階用 scatter/gather 讓 CFE 直寫 KS buffer）→ `KsStreamPointerAdvanceOffsetsAndUnlock` 送上層。
- **ks.h C++ 坑**：`avstream_dispatch.c`（純 C：KS descriptor/dispatch/callback）+ `hw_hal.cpp`（C++：RP1 暫存器/MMIO/I2C），`extern "C"` 標頭串接。

## 3. DeviceMFT 軟體 ISP

- **掛載**：AVStream `.inf` 用 `AddReg` 把 MFT CLSID 寫進 registry 的 `CameraDeviceMftCLSID`；App 開啟時 pipeline 自動把 DeviceMFT 插在 AVStream pin 上方。
- **轉換**：input `MEDIASUBTYPE_RP1_RAW10` → output `MFVideoFormat_NV12`。`ProcessOutput` 做 debayer：
  1. unpack RAW10（5 bytes→4×16-bit）。
  2. bilinear debayer（Bayer→RGB）。
  3. color matrix（RGB→YUV）+ 白平衡增益。
  4. 寫 NV12 Y/UV plane。（之後掛 NEON 優化。）
- **3A control loop**：
  1. `ProcessOutput` 掃 pixel 時順便累加 Y 算平均（AE）、累加 R/G/B 算比例（AWB）（參考 libcamera mean-luminance）。
  2. 算新 exposure/gain → MFT 呼叫 `IKsControl::KsProperty` 發自訂 `KSPROPERTY_RP1_CAM_EXPOSURE` 到 kernel。
  3. AVStream miniport 收到 → I2C 寫 IMX708 `0x0202`（integration time）/`0x0204`（gain）→ 下下張生效。

## 4. ★ PCIe DMA / IOMMU（WoA 生死線）

- **錯**：`MmAllocateContiguousMemory` + `MmGetPhysicalAddress` 塞 CFE → **WoA 會 SMMU fault / BugCheck `DRIVER_VERIFIER_DMA_VIOLATION`**。
- **對（WDF DMA）**：
  ```c
  WDF_DMA_ENABLER_CONFIG_INIT(&cfg, WdfDmaProfilePacket, 4096);
  WdfDmaEnablerCreate(Device, &cfg, ..., &DmaEnabler);
  WdfCommonBufferCreate(DmaEnabler, len, ..., &CommonBuffer);   // 如 10MB
  va  = WdfCommonBufferGetAlignedVirtualAddress(CommonBuffer);  // CPU 讀
  iova= WdfCommonBufferGetAlignedLogicalAddress(CommonBuffer);  // ★寫進 CFE DMA_BASE
  ```
- Windows HAL 自動設 SMMU 頁表把 IOVA 映到實體；RP1(PCIe) 只管對 IOVA 發 PCIe MemWrite。

## 5. ★ 5 步里程碑（嚴格按序，避免全黑地獄）

| Step | 動作 | 驗收 |
|------|------|------|
| **1 I2C+電源**（無關影像）| RP1 GPIO 拉高 XSHUTDN、供 EXTCLK、I2C 讀寫 | I2C 讀 `0x0000` 印出 **Chip ID `0x0708`** |
| **2 盲測 CFE DMA**（無關 AVStream）| 配 common buffer 填 CFE、I2C 下 stream ON | WinDbg 看 buffer 內容從 0 變雜訊 + frame-end 中斷觸發 = **硬體牆打穿** |
| **3 AVStream dummy 流**（無關 MFT）| 宣告假 NV12，DPC 不管 RAW 直接填全綠（Y150/U43/V21）| Windows 相機 App **穩定全綠畫面、FPS 正常** = AVStream 骨架通 |
| **4 MFT RAW 穿透 + 粗 debayer** | AVStream 改自訂 RAW GUID，掛 DeviceMFT，MFT 取 RAW10 高 8 bit 當灰階 Y、UV=128 | 相機 App 看到**有輪廓、會動的黑白畫面** = 整條 pipeline 零拷貝通 |
| **5 影像品質 + 3A** | 真 bilinear debayer + color matrix + KSPROPERTY 打回 AE/AWB | 顏色正常、**手電筒照鏡頭畫面自動變暗（AE 運作）** |
