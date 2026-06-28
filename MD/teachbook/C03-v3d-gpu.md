# C3：V3D GPU（WDDM 完整 GPU，工程量最大）

> 全書最難的一章——把一顆 GPU 在 Windows 上跑起來，等同重做一個顯卡驅動 + 使用者模式驅動。

| | |
|---|---|
| **裝置** | BCM2712 VideoCore VII / **V3D** GPU |
| **Linux 源碼** | `drivers/gpu/drm/v3d/v3d_drv.c`（KMD）+ Mesa（UMD，user space） |
| **Windows 框架** | **WDDM**（KMD + UMD 兩半） |
| **本專案** | 🔵 **KMD 可列舉空殼 + UMD winsys 後端 + 4 個 sim 模組**（見下方更新） |

> ⚠️ **進度更新（2026-06）**：本章原寫「只到骨架」，現已大幅推進（策略 `MD/Note/20260626-0200~0330`、現況 `RPi5-Porting-Status.md` #13）：
> - **策略定調 Vulkan-first**：寫一個 WDDM render KMD + port Mesa **v3dv**(Vulkan) 的 Windows winsys，上層 **Zink(GL)/DXVK·vkd3d(D3D)/clvk(OpenCL)** 全解鎖（不自寫 D3D UMD）。
> - **KMD（`gpu/rp1-v3d`）**：DxgkInitialize render-only 空殼——映射 V3D MMIO(hub/core0/sms 分開)+讀真實 IDENT、QueryAdapterInfo、BuildPagingBuffer(MMU flush)、SubmitCommand(寫 CT0/1 觸發)。**V3D ACPI 節點已加進 UEFI Dsdt**(`\_SB.GPU0`)列舉閉合。
> - **UMD（`gpu/v3dv-wddm`）**：v3dv 的 `v3d_ioctl` 加第三分支 → DRM ioctl 翻 D3DKMT（比照 v3d_simulator）。
> - **純邏輯模組 sim**：PTE encoder(10/10)、CL submit+MMU config(11/11，`MMU_CTL==0x060D0C01` 對上實機)、ioctl 翻譯(18/18)。**Pi5 實機校正** V3D 7.1.10.16。
> - **🔴 待實機**：D3DKMT 實接、monitored fence、render 核心、BSOD 除錯。

## 1. GPU 驅動是「兩半」

| 半 | Linux | Windows |
|----|-------|---------|
| 核心模式（KMD） | DRM driver（`v3d_drv.c`）：命令提交、記憶體、排程 | **WDDM KMD**（DxgkDdi…）：DMA buffer、GPU 排程、page table |
| 使用者模式（UMD） | **Mesa**（把 OpenGL/Vulkan 編成 V3D 指令） | **WDDM UMD**（D3D/OpenGL/Vulkan ICD） |

> 難點不只 KMD。**UMD 要把繪圖 API 編譯成 V3D 的指令**——Linux 用 Mesa 的 v3d 後端；Windows 上等於要把
> 那套 shader 編譯 + 命令產生**移植到 WDDM UMD**。這是工程量等同「重做一個 GPU 驅動」的來源。

## 2. 為什麼建議最後做（或先不做）

- 完整 3D 加速 = WDDM KMD + UMD（Mesa 移植）+ 排程 + 記憶體管理，巨大。
- **初期替代方案**：用 [DOD（C1）](C01-hdmi.md) 的 framebuffer + 軟體繪圖（WARP）就能有桌面，
  日常可用、不需 GPU 加速。
- 本專案已推進到「KMD 可列舉空殼（映射硬體/讀 IDENT/MMU·CLE 接線/ACPI 閉合）+ UMD winsys 翻譯層 + 4 個 sim 驗證模組」（見章首更新），
  但真正的端到端 3D 加速（D3DKMT 實接 + fence + Mesa v3dv build + Zink/DXVK）屬遠期 + 需實機。

## 3. 教學點

> 移植時要分清「**點亮畫面**（DOD，可早做）」與「**GPU 加速**（V3D，極難）」。很多人卡在後者，
> 但前者就能讓系統可用。**先求有畫面，再求快。**

➡️ 回 [目錄](README.md)
