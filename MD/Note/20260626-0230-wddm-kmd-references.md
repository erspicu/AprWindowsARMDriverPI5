# WDDM KMD（V3D render driver）參考資源與架構（2026-06-26）

> 來源：Gemini 諮詢（`tools/knowledgebase/message/20260626_014724.md`）+ 本專案 KMDOD 經驗。
> 策略總綱見 `20260626-0200-pi5-gpu-accel-stack.md`（Vulkan-first：KMD + v3dv UMD → Zink/DXVK/vkd3d）。

## render-only WDDM KMD 可行（不需 display DDI）
- WDDM 支援「render-only」設備（現代筆電：iGPU 顯示、dGPU 只 render）。我們的 V3D KMD **不需** VidPn/display DDI（顯示由 DOD/vc4 那條負責）。
- 與我們已做的 **DOD = KMDOD** 同框架（WDF + DxgkDdi）→ 拿 KMDOD 當骨架、砍 display、換 render 邏輯。

## 最小 DxgkDdi 集合
**必須**：
- 生命週期：`DxgkDdiAddDevice`/`StartDevice`/`RemoveDevice`/`Unload`
- 裝置/context：`DxgkDdiCreateDevice`、`DxgkDdiCreateContext`（對應 GPU queue）
- 記憶體(VidMm)：`DxgkDdiCreateAllocation`/`DestroyAllocation`、**`DxgkDdiBuildPagingBuffer`**
- 提交(VidSch)：**`DxgkDdiSubmitCommandVirtual`**（現代虛擬定址提交）
- 中斷/同步：`DxgkDdiInterruptRoutine`、`DxgkDdiDpcRoutine`、`DxgkDdiSignalMonitoredFence`
- 復原：**`DxgkDdiResetFromTimeout`**（TDR，強制）

**可先 stub**：所有 VidPn/display（`RecommendVidPnTopology`/`SetVidPnSourceAddress`…）——註冊時宣告不支援 display。
- ⚠️[需查證] WDDM 3.0 的 MCD（Microsoft Compute Driver，主給 NPU）可能不適用——跑 Vulkan 應註冊成標準 render graphics driver，查 WDK 對 MCD 限制。

## 關鍵參考資源
| 資源 | 用途 |
|------|------|
| **microsoft/Windows-driver-samples** `video/KMDOD` | 官方 KMDOD 骨架（DxgkDdi 初始化 + WDF 註冊樣板）。砍 display 當底。**我們 DOD 已用過同套。** |
| **virtio-win/kvm-guest-drivers-windows** `/viogpu`(新)`/viogpudo`(舊) | ⭐ **唯一開源、真實、穩定的 render WDDM KMD**：如何回應 dxgkrnl、記憶體分配、指令提交。重點讀其 `BuildPagingBuffer` / `SubmitCommand`。 |
| **Mesa** `src/gallium/winsys/d3d12/*`（Dozen/wgl）| UMD 端如何呼叫 `D3DKMT*`；我們要在 v3dv 寫類似的 `v3dv_winsys_wddm.c`（DRM ioctl ↔ D3DKMT）。 |
| WSLg / dxcore | dxgkrnl 在非傳統情境的路徑參考（[需查證] 適用度）。 |

## 提交工作流（UMD↔KMD）
1. UMD(v3dv) `D3DKMTCreateAllocation` 配 buffer 放 CLE 指令/狀態。
2. UMD 寫 CT0(binner)/CT1(render) 指令 → `D3DKMTSubmitCommand`/`SubmitCommandToHwQueue`。
3. dxgkrnl 排程 → 呼叫 KMD `DxgkDdiSubmitCommandVirtual`（給 GPU VA）。
4. KMD 把 VA 寫進 V3D `CTnCA`/`CTnEA` 暫存器 → 觸發硬體。
5. V3D 完成觸發 IRQ → `DxgkDdiInterruptRoutine` → 排 `DxgkDdiDpcRoutine`。
6. DPC 通知 Monitored Fence 達成 → UMD 得知完成。

## 記憶體：用 GpuMmu model（V3D 有自己的 MMU）
- V3D 自帶 MMU（GPU VA→PA，Linux 在 `v3d_mmu.c` 維護 PTE）。
- WDDM：VidMm 配實體記憶體+決定 VA → 呼叫 KMD `DxgkDdiBuildPagingBuffer`（夾 `DXGK_OPERATION_UPDATE_PAGE_TABLE`）。
- KMD 任務：把 OS 給的 PA 轉成 V3D PTE 格式、寫進 V3D page table、flush TLB（`V3D_MMU_CTL`）。
- **不走 IoMmu model**（那是給沒有自己 MMU、靠系統 SMMU 的硬體）。

## 排程 / TDR / Preemption
- **TDR 強制**：V3D 逾時（預設 2s）→ OS 呼 `DxgkDdiResetFromTimeout` → 必須 reset V3D core。
- **Preemption [需查證]**：WDDM 2.x 要求搶佔；V3D 硬體支援 job-level 中斷。**開發期 hack**：`DXGK_DRIVERCAPS` 宣告最粗粒度（DMA buffer boundary）搶佔，或延遲回應到當前 job 結束——只要不引發 TDR。

## 務實評估 + 雷區
- 難度極高（Gemini：全職 6-12 個月見「第一個三角形」、9.5/10）。
- 雷區：①WDDM 囉嗦（Linux v3d_gem/sched 幾千行 → WDDM 上萬行才不 BSOD）②`BuildPagingBuffer` 文件晦澀、欄位語意需逆向/猜 ③Monitored Fence/Hardware Queue 同步狀態機極易死結。
- 先例：Apple Silicon Windows 3D 仍多軟體渲染；Freedreno→WDDM 少人嘗試（WoA 都用 Qualcomm 閉源）。

## 我們的第一步（when 接實機）
1. 雙機 KDNET（Pi5 被除錯機 + WinDbg 主機）。
2. 拿 KMDOD 砍 display → 做「Device Manager 顯示 V3D Render Device、掛載不 BSOD」的空殼。
3. 精讀 viogpu 的 `BuildPagingBuffer`/`SubmitCommand`，對上 V3D CLE。
- 對應暫存器（CT0/1CA/EA、MMU_CTL）見 `20260625-0200` V3D 段；藍圖 `MD/Note/gpu/`。

## 能力定位（本專案）
AI 可做：render-only KMD 骨架、DxgkDdi callbacks、V3D 提交/MMU/TDR 邏輯、對照 viogpu/KMDOD（DDI 簽名對真 WDK header 核對）→ 推到 🔵（編譯+結構完整）。功能驗證（paging/fence BSOD 除錯）需 Pi5 實機 + KDNET 互動 WinDbg（需人在場）。
