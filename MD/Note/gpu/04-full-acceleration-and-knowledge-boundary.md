# V3D GPU：玩家完整加速 🎮 + 知識邊界誠實劃分

> 玩家要的是「能打 3D 遊戲」。本篇給 Phase 3 完整加速底層藍圖、**Gemini 誠實劃出的知識邊界**、V3D 開源真相，
> 以及「**不綁微軟 D3D、給玩家一個能用的加速窗口**」的最務實路線。承 [`03`](03-acceleration-mesa-path.md)。

## ★ 核心哲學：不必走原生 D3D，直接給 Vulkan 窗口

```
                          ┌── 原生 Vulkan 遊戲/模擬器/vkcube ──► 零翻譯直接跑
遊戲 ─┤
      └ D3D 老遊戲 ─► DXVK / VKD3D-proton（D3D→Vulkan）─┐
                                                        ▼
                        Mesa v3dv（Vulkan ICD，port 到 Windows）
                        → 自寫 Winsys → 精簡 WDDM render KMD（記憶體/提交/中斷）→ V3D 7.1 硬體
```

- **不寫原生 D3D UMD**（避開最大地獄：DXIL→QPU 編譯器）。
- **出 Vulkan 即可**：支援 Vulkan 的東西直接跑；D3D 遊戲用 DXVK/VKD3D 翻譯（同 Steam Deck/Proton/Asahi 打 Windows 遊戲的招）。
- 玩家拿到的「加速窗口」= **一張能用的 Vulkan ICD**。這就夠了。

## 【A】Phase 3 完整 WDDM render 底層藍圖

> 把 Windows 的記憶體/排程模型（Dxgkrnl）翻成 V3D 硬體語言。**暫存器確切 offset 一律查 Mesa `v3d` source，勿憑記憶**。

### 1. VidMm（Video Memory Manager）
- `DxgkDdiCreateAllocation`：UMD 要記憶體時 KMD 描述大小/對齊（V3D 通常 4KB 分頁）。
- `DxgkDdiBuildPagingBuffer`：核心；Dxgkrnl 傳 paging 動作（Transfer/Map/Unmap）。
- `DxgkDdiMapGpuVirtualAddress`（WDDM 2.0+）：給一段 GPU VA + 實體分頁陣列。
- **V3D 對接**：V3D 有獨立 MMU（與 A76 MMU 分開）；驅動維護 V3D 的 Page Directory/Table，`Map` 時把實體位址寫進 PTE（加 V3D 屬性 bit），寫完**對 V3D MMU 控制暫存器下 TLB flush**。

### 2. 排程與命令提交（V3D = Tile-Based Deferred Renderer）
- `DxgkDdiCreateContext`：建硬體 context，分專屬 GPU VA（切 MMU page directory base）。
- `DxgkDdiSubmitCommand`：收 UMD 的 command buffer。
- **V3D CLE（Control List Executor）觸發**：
  - Binner（算頂點落在哪個 tile，**CT0**）+ Render（逐 tile 渲染，**CT1**）。
  - 把 binner CL 起始 GPU VA 寫 `CLE_CT0CA`（current addr）、結束寫 `CLE_CT0EA`（end addr）；**一旦 EA>CA，CLE 就醒來開吃**。Render（CT1）同理，通常由 binner done 中斷/硬體同步觸發。
  - **hang 偵測**：讀 `CLE_CT0CA` 是否停滯，或靠 Windows timeout。
- `DxgkDdiPreempt` / `DxgkDdiPatch`：搶占 / 位址修補。

### 3. TDR（Timeout Detection & Recovery）
- `DxgkDdiResetFromTimeout` / `ResetEngine`：Windows 發現 fence 遲遲沒回就呼叫。
- **V3D 軟 reset**：寫 V3D HUB 的 reset bit → 等完成 → 重建 MMU、把闖禍 context 標 guilty 砍掉、無辜 context 重新丟回 CLE。

### 4. 中斷
- `DxgkDdiInterruptRoutine`（DIRQL，要快）：讀 V3D 中斷狀態 → binner/render done（清 flag、觸發 DPC）；MMU fault（記 VA+原因，通常觸發 TDR）。
- DPC → `DxgkCbNotifyInterrupt`（告訴 Windows 哪個 fence ID 完成）。

### 5. 同步（monitored fence）
- Windows 給 GPU VA + 64-bit 值；UMD 在 CL 尾端插 V3D 同步封包，要 V3D 渲染完把該值寫進那 VA 並觸發 IRQ；OS 收中斷後檢查記憶體確認完成。

## 【B】知識邊界（Gemini 誠實劃分）

把 Phase 3 還不能「照著打就跑」的點，分三類：

| 類別 | 內容 |
|------|------|
| **(a) 廠商鎖死/需逆向** | ① **電源/時脈管理（DVFS）**：V3D 7.1 在 Pi5 的調頻極依賴 Pi 韌體（RP1 + VideoCore），Windows 端怎麼透過 Mailbox/SCMI 調 GPU 時脈 + ACPI/PEP(Power Engine Plugin) 綁定**未公開、嚴重缺文件** ② **硬體 errata**：官方驅動偷偷 workaround 的硬體 bug，沒 Broadcom 內部文件只能遇 hang 抓瞎 |
| **(b) 訓練資料不足（會編造，須查證）** | ① **V3D 7.1 確切暫存器 offset**：知道 `CLE_CT0CA` 存在，但**無法憑空寫對 hex 位址**——查 Mesa `v3d_regs.h`/Linux ② **Windows ARM64 WDDM SoC UMA 的 cache coherency 細節**：微軟對 x86 PCIe GPU 文件海量，對 ARM64 SoC 統一記憶體的快取一致性文件稀缺 |
| **(c) 只是工程量大（資料齊全）** | KMD 狀態機（V3D 中斷/CLE 排程塞進 WDDM context）、WDDM paging 翻成 V3D MMU 格式——純苦力、無秘密、但極易藍屏 |

## 【B】V3D 開源真相（最有把握的事實）

> **相較 Mali/Adreno/PowerVR，V3D 是「最適合也最容易」被社群移植到 WDDM 的 GPU。**

- Broadcom **沒**公開 V3D 7.1 PDF 手冊（只放過舊版 2.x/3.x），**但**官方僱的工程師（Igalia + Pi 基金會）把**所有硬體細節以程式碼形式貢獻進 Mesa**。
- **公開程度**：暫存器定義 **100%**（Mesa XML/C header）、CL 封包格式 **100%**、QPU ISA **100%**（完整編譯器）、MMU 格式 **100%**（Linux DRM）。
- **為什麼比別人好**：Mali=Panfrost 逆向、Adreno=Freedreno 逆向；**V3D 的 `v3d`/`v3dv` 就是原廠官方驅動**，程式碼邏輯就是最正確的硬體操作指南。
- **真正還鎖死的**：只有 firmware 溝通介面 + 底層電源管理（通常 Pi Linux BSP 處理，移到 Windows 要對 Pi UEFI 韌體做適配）。

## 【C】誠實結論：玩家能不能拿到完整加速？

| 目標 | 能否 | 說明 |
|------|------|------|
| **原生 D3D12/D3D11 WDDM 完整驅動** | ❌ **不能** | 小團隊在沒微軟/Broadcom 金援下，手寫原生 UMD + **DXIL→QPU 編譯器**＝數十人×數年，不切實際 |
| **玩家有 GPU 加速可用（Vulkan + DXVK）** | ✅ **能！極有希望** | 精簡 WDDM KMD（記憶體/提交/中斷）+ 移植 Mesa `v3dv`(Vulkan)/`v3d`(GL) 到 Windows + 自寫對接 KMD 的 Winsys + **DXVK/VKD3D** 翻 D3D。**硬體細節因開源全亮，缺的只是跨平台接線工 + 毅力** |

- **最大單一阻礙**＝「DXIL→QPU 編譯器」，但 **DXVK/Vulkan 路線正好繞過它**（遊戲→D3D→DXVK→Vulkan→v3dv，編譯由 Mesa 包辦）。
- **這是玩家拿到完整加速的唯一且最快途徑。**

## 落地順序（玩家加速）

> **加速窗口出 GL 或 Vulkan 皆可**（Mesa `v3d`→OpenGL ICD、`v3dv`→Vulkan ICD），兩個都是原廠官方驅動。
> 先出哪個看你要先驗哪條：GL ICD 最快讓 `glxgears`/舊遊戲跑；Vulkan ICD 接 DXVK 涵蓋最多現代 D3D 遊戲。

1. 先 [`02`](02-dod-implementation.md) DOD 點亮桌面。
2. [`03`](03-acceleration-mesa-path.md) Phase 2 保命路線：極簡 KMD + Mesa winsys，**硬體 clear buffer 成紅色**驗證 GPU 通。
3. 把 Mesa build 成 Windows ICD（**`v3d`→OpenGL 或 `v3dv`→Vulkan，擇一先做**），接上 KMD → **`glxgears`/`vkcube` 跑起來** = 第一個 GPU 加速畫面。
4. 升級成 WDDM render miniport（VidMm/排程/TDR）讓 ICD 能被標準 GL/Vulkan loader 發現 + present/DWM。
5. （要涵蓋 D3D 遊戲）掛 **DXVK/VKD3D**（走 Vulkan）；純 GL/Vulkan 遊戲與模擬器則直接跑。
