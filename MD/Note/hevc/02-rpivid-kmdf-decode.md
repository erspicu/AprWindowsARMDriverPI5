# HEVC 移植：rpivid KMDF driver + stateless 解碼流程

> 承 [`01`](01-hardware-and-reality.md)。**Broadcom 未公開 rpivid 暫存器手冊**——以下硬體行為基於 Linux
> `drivers/staging/media/rpi/rpivid/` 的理解；**確切 MMIO offset / descriptor bitmask 一律標「需查 Linux rpivid source」**。

## 1. KMDF driver 骨架

### MMIO + ACPI
- rpivid 在 DT 通常含：`intc`（特有中斷控制區塊）+ `hevc`（核心暫存器）+ clock/reset。
- DSDT 定義虛擬裝置（如 `RPIV0001`），把這幾段 `Memory32Fixed` + `Interrupt` 包進去。
- `EvtDevicePrepareHardware`：走訪 `WdfCmResourceList` 找 `CmResourceTypeMemory` → `MmMapIoSpaceEx`（`PAGE_NOCACHE`/`PAGE_WRITECOMBINE`）。**MMIO base/offset 需查 Linux source**。

### decode 提交模型（descriptor chain）
1. 把控制結構（Phase 1/2 command buffers）+ slice data 的**實體位址**寫進特定 MMIO 暫存器。
2. 寫控制暫存器的 **START bit**（offset 需查）。
3. 硬體 DMA 讀 → 解碼 → 寫出 SAND 圖像 → 拉 IRQ。

### IRQ → user-mode（Pended IOCTL）
- `EvtInterruptIsr`：讀中斷狀態確認 decode done、寫 ACK 清中斷（offset 需查）、回 TRUE 觸發 DPC。
- `EvtInterruptDpc`：從 WDFQUEUE 取 pending IOCTL，`WdfRequestComplete` 回 MFT。

### IOCTL 介面建議
```c
#define IOCTL_RPIVID_ALLOC_DMA_BUFFER  CTL_CODE(FILE_DEVICE_UNKNOWN,0x801,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_RPIVID_SUBMIT_AND_WAIT   CTL_CODE(FILE_DEVICE_UNKNOWN,0x802,METHOD_BUFFERED,FILE_ANY_ACCESS) // pend 到 IRQ
struct RPIVID_SUBMIT_REQ {
    UINT64 bitstream_pa;     UINT32 bitstream_size;
    UINT64 config_struct_pa; // 硬體 descriptor 實體位址
    UINT64 reference_pas[16];// DPB 參考幀實體位址表
    UINT64 output_pa;        // 輸出（SAND）緩衝區
};
```

## 2. Stateless 解碼流程（V4L2 → 硬體 descriptor）

> 精神：硬體是瞎子，host 餵嚼碎的指令。Linux 把 V4L2 controls 重新打包成 rpivid 的 Phase 1/Phase 2 hardware descriptors。

| V4L2 stateless control | 內容 | 對應 |
|------------------------|------|------|
| `V4L2_CID_STATELESS_HEVC_SPS` | pic width/height、CTU size、bit depth | descriptor header 欄位（查 `rpivid_h265_setup` 類函數的 bitmask/shift，如 `(w<<16)\|h`）|
| `..._PPS` | picture 參數 | 同上 |
| `..._SCALING_MATRIX` | 自定義 scaling list | 開一塊實體記憶體放，PA 寫進 descriptor |
| `..._SLICE_PARAMS` | per-slice | descriptor per-slice 欄位 |
| `..._DECODE_PARAMS` | DPB/RPS | reference address table |

- **slice data**：推測 rpivid 可吃含 start code 的 Annex B 連續 NALU（硬體內部找 start code）；或需去 emulation prevention bytes 的 RBSP——**需查 source**。

## 3. DPB / reference frame 管理

- **MFT(user-mode) 維護 DPB manager**：每解一張放進 DPB list。
- **RPS 映射**：slice header 的 L0/L1 存 POC；硬體只認 index(0~15) → MFT 查 DPB 找該 POC 的 frame buffer **PA** → 填進 `reference_pas[16]`。
- output frame buffer 由 MFT 從 common buffer pool 配。

## 4. SAND 輸出格式 + 轉換

- **SAND**：column-based tiled，通常 **128-byte 寬垂直條**，Y / UV 平面分開（macroblock-tiled）。查 `V4L2_PIX_FMT_NV12_COL128` / `SAND128` 定義。
- **SAND → linear NV12**：寫 ARM64 **NEON** 函數，按 column 寬度定址把 tile 搬成逐行 NV12。
- **ISP 硬體轉？** Linux 交給 Pi ISP；**Windows 無 ISP 驅動**（要轉就得再寫一個 ISP KMDF）。→ **早期強烈建議 CPU NEON 軟轉**；未來可考慮 GPU compute shader detile（但需 WDDM）。

## 5. 記憶體（CMA 等價）

- 所有 buffer（bitstream / reference / output SAND / descriptor）都要**連續實體記憶體**。
- **別用** `MmAllocateContiguousMemory`（跑久碎片化，4K 大塊會配失敗）。
- **最佳**：`EvtDriverDeviceAdd` 初始化 `WdfDmaEnabler` + `WdfCommonBufferCreate` 預配大池（≈ Linux CMA）。
- **4Kp60 容量**：1 張 4K NV12(8-bit)=3840×2160×1.5≈**12.4MB**；HEVC 最大 DPB 16 張≈**198MB**；加 output queue + bitstream → **一次霸佔 ≥256MB common buffer** 給 MFT 自管。

## 6. 第一個里程碑（先別寫 MFT）

console C++ + KMDF：
1. Driver：ACPI 解析、MMIO 映射、IRQ 註冊、一個 IOCTL 收 PA → 寫 MMIO 觸發 START → IRQ → complete。
2. App：FFmpeg 抽一幀 **1080p（先別 4K）I-frame** RAW bitstream；對照 Linux ioctl log 寫死該 frame 的 Phase 1/2 descriptor。
3. 流程：App 要 DMA buffer → 複製 bitstream+descriptor 進去 → submit IOCTL → driver 寫暫存器啟動 → IRQ 返回 → App 讀回 output（SAND）。
4. 驗證：C 迴圈把 SAND 轉 linear NV12 → 存 `.yuv` → PC 上 YUV player 開，看到清晰畫面（哪怕只有 Y 正確都是巨大成功）。
