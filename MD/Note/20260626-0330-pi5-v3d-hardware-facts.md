# Pi5 V3D GPU 硬體真相（2026-06-26 實機唯讀萃取）

> SSH 唯讀讀 `/sys/kernel/debug/dri/*/v3d_ident` + `v3d_regs`（不寫、不碰硬體狀態）。校正 KMD/UMD。
> 接續 `20260626-0200`(stack)/`0230`(KMD)/`0300`(UMD port)。

## V3D 版本/能力（debugfs v3d_ident）
- **Revision 7.1.10.16**；Core0：**Revision 7.1, Slices 3, TMUs 3, QPUs 12, Semaphores 0**。
- **MMU: yes**（自帶 GPU MMU → WDDM GpuMmu model）、**MSO: yes**、**L3C: no**。
- ⚠️ **TFU: no**（無 Texture Formatting Unit）→ `DRM_V3D_PARAM_SUPPORTS_TFU` 應回 **0**（我們原本誤設 1，已校正）。

## 原始 IDENT 暫存器（v3d_regs debugfs）— 填 KMD/UMD placeholder 用
**HUB block**（base 0x1002000000）：
| reg | off | 值 |
|-----|-----|----|
| HUB_UIFCFG | 0x04 | 0x00000045 |
| **HUB_IDENT0** | 0x08 | **0x42554856**（"VHUB"）|
| HUB_IDENT1 | 0x0c | 0x00081117 |
| HUB_IDENT2 | 0x10 | 0x00001900 |
| HUB_IDENT3 | 0x14 | 0x00020a10 |
| **MMU_CTL** | 0x1200 | 0x060d0c01（TLB_CLEAR=BIT2）|

**Core0 block**（base 0x1002008000）：
| reg | off | 值 |
|-----|-----|----|
| **CTL_IDENT0** | 0x00 | **0x07443356**（"V3D"+ver；VER=bits[31:24]=0x07=7）|
| CTL_IDENT1 | 0x04 | 0x81001431 |
| CTL_IDENT2 | 0x08 | 0xc0078101 |
| CTL_MISCCFG | 0x18 | 0x00000006 |
| CLE_CT0CA/EA、CT1CA/EA | 0x110/0x108、0x114/0x10c | 0（idle）|

## ⚠️ 關鍵：hub 與 core0 是分開的 MMIO 區塊
DT `v3d@2000000`：hub @0x1002000000(0x4000)、**core0 @0x1002008000**(0x6000)、sms @0x1002030800(0x700)。
**HUB_* 相對 hub base；CTL/CLE/MMU 相對 core base。** → KMD 不能只映一個窗口讀兩者。
**已修正**：`rp1-v3d` KMD StartDevice 依 ACPI _CRS 順序分別映射 3 區塊（HubRegs/CoreRegs/SmsRegs），HUB_IDENT0 讀 hub、CTL_IDENT0 讀 core。

## 已套用的校正
- KMD `common.h`：hub/core/sms 三指標 + accessor 吃 base；`ddi.c` StartDevice 分別映射、讀對的區塊。
- UMD `v3dv_wddm.c`：`d3dkmt_open_adapter` 種真實 IDENT 值；GET_PARAM **SUPPORTS_TFU=0**；sim 加 IDENT0==0x07443356 斷言（16/16）。

## Vulkan（v3dv 在 Pi5 報的，供 UMD 對照）
- `apiVersion 1.3.305`、`driverName V3DV Mesa`、`deviceName V3D 7.1.10.2`。
- 後續可在 Pi5 跑 `vulkaninfo` 完整 dump 對照我們 UMD 要回報的 features/limits。

## 待續（需實機）
- KMD `QueryAdapterInfo` private → 把真 IDENT 傳給 UMD（取代種子值）。
- V3D 中斷（GIC SPI 250/249）實際 ISR；CLE 提交後 fence。
