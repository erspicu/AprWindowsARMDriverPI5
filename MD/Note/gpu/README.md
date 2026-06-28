# V3D GPU（VideoCore VII）移植筆記（早期研究）

> ⚠️ **這是 2026-06-22 的早期研究/方向評估。實作進度與最新策略請看**：
> `../20260626-0200`(策略總綱)、`-0230`(WDDM KMD 架構/參考)、`-0300`(v3dv UMD port 面)、`-0330`(V3D 實機 facts)；
> 程式碼 `windows_sources/gpu/rp1-v3d`(KMD)、`gpu/v3dv-wddm`(UMD)；現況見 `RPi5-Porting-Status.md` #13 + `MD/HANDOFF.md` §8。
> 本檔當初預判的 **Vulkan(v3dv)+DXVK** 路線＝後來實際採用的主線（方向正確）。
>
> Pi5（BCM2712，GPU = Broadcom **VideoCore VII / V3D 7.1**）的 GPU 移植到 **Windows on ARM64** 的研究筆記。
> 來源：本專案分析 + Gemini 3 輪諮詢（`tools/knowledgebase/message/`）。建立：2026-06-22

## 務實判斷（最重要，先看這個）

兩種目標、兩條路：

| 你要的 | 路線 | 工程量 | 結論 |
|--------|------|--------|------|
| **只要可用桌面** | **DOD + WARP**（CPU 軟體渲染）| 1-2 月 | Cortex-A76 ×4 夠力，瀏覽/文書綽綽有餘。**ROI 最高** |
| **玩家要 3D 遊戲加速** | **精簡 WDDM KMD + Mesa v3dv(Vulkan) + DXVK/VKD3D** | ~1 人年（小團隊可行）| **能做！社群實證路線**（同 Steam Deck/Asahi 打 Win 遊戲）|
| 原生 D3D12 WDDM 驅動 | 自寫 UMD + DXIL→QPU 編譯器 | 數十人×數年 | ❌ 小團隊不切實際 |

> **V3D 是「最適合開源 WDDM 移植」的 GPU**：Mesa 的 `v3d`/`v3dv` 就是 **Broadcom 官方工程師寫的官方驅動**
> （非逆向）。暫存器/CL 封包/QPU ISA/MMU 格式 **100% 公開**；真正鎖死的只有 firmware/電源管理。
> 詳見 [`04-full-acceleration-and-knowledge-boundary.md`](04-full-acceleration-and-knowledge-boundary.md)。

## 玩家加速路線（能打遊戲的那條）🎮

```
遊戲(D3D11/12) → DXVK/VKD3D(D3D→Vulkan) → Mesa v3dv(Vulkan ICD, port 到 Windows)
              → 自寫 Winsys → 精簡 WDDM KMD(記憶體/提交/中斷) → V3D 7.1 硬體
```
- **不寫原生 D3D UMD**（最大地獄 DXIL→QPU 編譯器被這條路繞過）。
- 缺的只是「跨平台接線工 + 毅力」，硬體細節因開源全亮。

## 階段（Phase）

| Phase | 內容 | 工程量 | 換到什麼 |
|-------|------|--------|----------|
| **1. DOD + WARP** | 點亮 HDMI + CPU 軟體渲染桌面 | 1-2 月 | **可用桌面** |
| 2. Mesa 獨立驗證 | 極簡 KMD + Mesa winsys，跑硬體 clear/compute | 3-5 月 | 證明 GPU 通了 |
| 3. 完整加速 | WDDM KMD + Mesa v3dv + DXVK | 6-12 月 | **遊戲硬體加速** |

## 筆記索引

| 檔案 | 內容 |
|------|------|
| [`01-strategy-and-references.md`](01-strategy-and-references.md) | WDDM 架構（DOD vs render、KMD/UMD 分工）、Mesa 重用決策、工程量/風險、**開源參考**（viogpu、WoR Pi4 wddm、Mesa、KMDOD sample）|
| [`02-dod-implementation.md`](02-dod-implementation.md) | **Phase 1 DOD 可落地**：KMDOD DDI、**Hybrid（Mailbox modeset + MMIO flip）**、framebuffer/VSync/EDID、**里程碑 M1-M4（含 UEFI GOP framebuffer 劫持 = 最快看到桌面）**|
| [`03-acceleration-mesa-path.md`](03-acceleration-mesa-path.md) | **Phase 2/3 加速**：Windows build Mesa v3d、Winsys 介面對應 D3DKMT、d3d10umd、**保命路線（極簡 KMD + 硬體 clear 驗證）**、shader chain、Render KMD/VidMm 風險 |
| [`04-full-acceleration-and-knowledge-boundary.md`](04-full-acceleration-and-knowledge-boundary.md) | **玩家完整加速 🎮**：Phase 3 WDDM render 底層藍圖（VidMm/CLE/TDR/中斷/fence）+ **知識邊界誠實劃分**（廠商鎖死 vs 訓練不足 vs 工程量）+ V3D 開源真相 + **DXVK 玩家路線**誠實結論 |

## 現況
專案有 DOD 骨架 `rp1vc4dod.sys`（KMDOD 式，19 DDI stub）+ V3D render 骨架。下一步最務實 = 把 DOD 推到「看到桌面」（[02](02-dod-implementation.md) 的 M3）。

## 最快「看到桌面」的捷徑
**劫持 UEFI GOP framebuffer**：開機 bootloader 已點亮 HDMI（Windows 轉圈圈那個畫面）。DOD 的 `SetVidPnSourceAddress`
裡直接 `memcpy` 把 WARP 畫好的 surface 複製到那塊 GOP framebuffer 實體位址——**連 BCM2712 顯示暫存器都不必碰**，
螢幕就會出現 Windows 桌面（雖然 memcpy 慢，先求有畫面）。詳見 [02](02-dod-implementation.md)。
