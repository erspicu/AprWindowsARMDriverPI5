# Pi5 實機萃取：顯示（vc4/HVS/HDMI）硬體真相（2026-06-25）

> SSH 唯讀萃取 Pi5 Linux（kernel 6.18），校正 DOD/GPU 顯示移植。連線法見 `MD/Skill/pi5-ssh-hardware-facts.md`。

## BCM2712 顯示 block 位址（SoC @ 0x107c000000）
| Block | 位址 | Linux 綁定 | 備註 |
|-------|------|-----------|------|
| **HVS**（Hardware Video Scaler）| `0x107c580000` | `vc4_hvs_ops` | **在 BCM2712 IOMMU 後**（`1000005200.iommu`，group 1）→ HVS DMA 經 IOMMU |
| **HDMI0** | `0x107c701400` | `vc4_hdmi_ops` | rc0/sound card0 |
| **HDMI1** | `0x107c706400` | `vc4_hdmi_ops` | rc1/sound card1 |
| **MOP** | `0x107c500000` | `vc4_txp_ops` | Pi5 新輸出管線（transposer/writeback 類）|
| **MOPLET** | `0x107c501000` | `vc4_txp_ops` | 較小的 pipeline |
| **V3D**（GPU render）| `0x1002000000` | `v3d` drm minor 0 | 與顯示分離（render-only）|

## 其他
- DRM：`card0`(v3d render) + `card1`(vc4 display，connectors HDMI-A-1/2 + Writeback-1/2)。
- 開機 cmdline：`video=HDMI-A-1:2560x1440@60D`（forced on），`console=ttyAMA10,115200`。
- **VideoCore 保留記憶體**：`vc_mem.mem_base=0x3fc00000  vc_mem.mem_size=0x40000000`（mailbox/firmware framebuffer）。
- 目前 fb0：2560×1440。

## 對 DOD 實作的影響
- **HVS 在 IOMMU 後** → DOD 若直接 MMIO 驅 HVS scanout，framebuffer DMA 要過 BCM2712 SMMU（同 IOMMU 筆記；ACPI IORT 要描述）。→ 更印證「Hybrid：先靠 VideoCore 韌體 mailbox 配 fb + modeset，DOD 只 flip」的策略。
- HDMI0/HDMI1 雙位址確認 → 多 head（[`display-outputs/`](gpu/../display-outputs/) 或 `MD/Note/display-outputs/`）的 base/IRQ 各自獨立。
- 這些位址供 Phase C/D（CommitVidPn modeset、HVS flip）實機填暫存器用。

## vc4 暫存器 offset（從本機 `sources/drivers/gpu/drm/vc4/` 權威源碼）
> ⚠️ Pi5（BCM2712）是 gen5/vc6 HVS——部分用 **`SCALER5_*`** 變體，offset 可能與下列舊 `SCALER_*` 不同；
> 用時對照 `vc4_regs.h` 的 SCALER5 段 + `vc4_hvs.c` 的 Pi5 路徑確認。

**HVS（SCALER，base 0x107c580000）**：
| reg | offset |
|-----|--------|
| `SCALER_DISPCTRL` | `0x00` |
| `SCALER_DISPSTAT` | `0x04` |
| `SCALER_DISPLIST0/1/2` | `0x20`/`0x24`/`0x28`（`DISPLISTX(x)=0x20+4x`，**寫 display-list 指標**）|
| `SCALER_DISPBASE0/1/2` | `0x4c`/`0x5c`/`0x6c` |

**HVS display-list 控制字 `SCALER_CTL0`（每個 plane 的 dlist 起始字）**：
`END=bit31`、`VALID=bit30`、`SIZE=[29:24]`、`TILING=[21:20]`（LINEAR=0/64B=1/128B=2/256B_or_T=3）、`ALPHA=bit19`、`HFLIP=bit16`。後續字含 pixel format、position(POS0/POS2 寬高)、framebuffer 實體位址（DISPBASE/ptr）。詳見 `vc4_plane.c vc4_plane_mode_set`。

**HDMI（base HDMI0 0x107c701400 / HDMI1 0x107c706400）**：`vc4_hdmi_regs.h` 用 **enum + per-variant offset 表**（`HDMI_HOTPLUG`、`HDMI_HORZA/HORZB`(時序)、`HDMI_FIFO_CTL`、`HDMI_SCHEDULER_CONTROL`、`HDMI_CSC_*`、`HDMI_TX_PHY*`）。Pi5 用哪張表查 `vc4_hdmi.c` 的 variant（bcm2712）。

> 結論：DOD 的 **HVS flip** 最少只需寫 `SCALER_DISPLISTX` 指到組好的 dlist（含 framebuffer 實體位址）；modeset（HDMI 時序/PHY）建議仍走 mailbox 韌體（Hybrid 策略）。
