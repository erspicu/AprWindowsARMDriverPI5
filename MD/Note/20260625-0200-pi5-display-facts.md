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
- 這些位址供 Phase C/D（CommitVidPn modeset、HVS flip）實機填暫存器用；**確切暫存器 offset 仍需對 Linux `vc4_hvs.c`/`vc4_hdmi.c`**。
