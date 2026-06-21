# C3：V3D GPU（WDDM 完整 GPU，工程量最大）

> 全書最難的一章——把一顆 GPU 在 Windows 上跑起來，等同重做一個顯卡驅動 + 使用者模式驅動。

| | |
|---|---|
| **裝置** | BCM2712 VideoCore VII / **V3D** GPU |
| **Linux 源碼** | `drivers/gpu/drm/v3d/v3d_drv.c`（KMD）+ Mesa（UMD，user space） |
| **Windows 框架** | **WDDM**（KMD + UMD 兩半） |
| **本專案** | render 骨架（`windows_sources/gpu/rp1-v3d/`，`DxgkInitialize`），🔲 |

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
- 本專案只到「WDDM render 骨架可編譯」（`DRIVER_INITIALIZATION_DATA` + `DxgkInitialize`），
  真正加速屬遠期 + 需實機 + 大框架。

## 3. 教學點

> 移植時要分清「**點亮畫面**（DOD，可早做）」與「**GPU 加速**（V3D，極難）」。很多人卡在後者，
> 但前者就能讓系統可用。**先求有畫面，再求快。**

➡️ 回 [目錄](README.md)
