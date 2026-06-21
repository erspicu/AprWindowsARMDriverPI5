# HEVC 移植：MFT 實作 + 註冊 + 整合現實

> 承 [`02`](02-rpivid-kmdf-decode.md)（KMDF 已能解出 SAND frame）。把它包成 user-mode MFT 讓 Windows 播放器用。

## 1. IMFTransform 實作（建議 Sync MFT）

> Async MFT 規範繁瑣又易與第三方 splitter 不相容 → **做 Sync MFT**。

必實作方法：
- `GetStreamLimits` / `GetStreamCount`：1 in, 1 out。
- `GetInputAvailableType`：回 `MFVideoFormat_HEVC`；`GetOutputAvailableType`：回 `MFVideoFormat_NV12`。
- `SetInputType` / `SetOutputType`：系統選定格式後 init KMDF session。
- `GetInputStreamInfo` / `GetOutputStreamInfo`：宣告 buffer 大小/alignment。
- `ProcessMessage`：處理 `MFT_MESSAGE_COMMAND_FLUSH`（seek 清 DPB）、`..._NOTIFY_BEGIN_STREAMING`。
- `ProcessInput`（收壓縮）/ `ProcessOutput`（吐 NV12）。

**Type**：
- Input：`MF_MT_MAJOR_TYPE=MFMediaType_Video`、`MF_MT_SUBTYPE=MFVideoFormat_HEVC`。**注意 bitstream 兩種**：Annex B（start codes）vs HVCC（MP4 length-prefixed）——`SetInputType` 要判斷你的 KMDF 吃哪種。
- Output：`...=MFVideoFormat_NV12` + `MF_MT_FRAME_SIZE`（由 SPS 解析的解析度）。

**ProcessInput/Output 流程（非 1-to-1，因 B-frame/reorder）**：
1. `ProcessInput`：收 `IMFSample`（HEVC 壓縮）→ 抽 buffer → `DeviceIoControl` 餵 KMDF（此時可能還不能輸出）。
2. `ProcessOutput`：問 KMDF「有沒有解好且排序好的畫面」？
   - 有 → SAND→NV12 塞 `pOutputSamples`，回 `S_OK`。
   - 沒有（DPB 未滿）→ 回 **`MF_E_TRANSFORM_NEED_MORE_INPUT`**（告訴上游再餵）。
- **reorder 放哪**：最佳＝VPU firmware 自管 DPB 並按 POC 吐（MFT 無腦 poll）；最壞＝firmware 按 decode order 吐，MFT 自己維護 DPB、算 POC、等 B-frame 到齊再按序吐。

## 2. 註冊成系統 HEVC MFT

- `MFTRegister`（全域，寫 registry），category `MFT_CATEGORY_VIDEO_DECODER`，宣告 input `MFVideoFormat_HEVC` / output NV12。
- **★ flag 陷阱**：**絕對不要 `MFT_ENUM_FLAG_HARDWARE`**（代表 Async + 綁 PnP + 預期輸出 D3D surface zero-copy；沒 WDDM 會 crash 或被忽略）。
- **正確**：用 **`MFT_ENUM_FLAG_SYNCMFT`**，把自己**偽裝成「超高效軟體 decoder」**（實則內部叫 KMDF 硬解）＝"system-memory based HW-accelerated MFT"。
- **merit**：微軟管很嚴；最直接測試＝**解除安裝微軟「HEVC 視訊延伸模組」**，系統就只剩你的 MFT 可選。

## 3. ★ 整合現實（最殘酷但決定價值）

| 對象 | 會用？ | 原因 |
|------|:---:|------|
| **電影與電視 / WMP / UWP / `IMFMediaEngine`** | ✅ **會** | 純 Media Foundation；MFT 註冊成功+吐標準 NV12 → topology builder 自動接上 EVR。代價：NV12 在 system memory，EVR 要 CPU upload 到 GPU texture（4K 有負載但 Pi5 CPU 扛得住）|
| **Edge / Chromium（YouTube）** | ❌ **絕對不會** | Chromium 繞過 MFT，直接 D3D11 query `D3D11_VIDEO_DECODE_PROFILE_HEVC_MAIN`(DXVA)；沒 WDDM=沒 D3D11VideoDevice → 它根本不知道你能硬解。WARP 不支援 video decode profile，`MF_SA_D3D11_AWARE` 也救不了（要有 D3D device 建 surface）。**死胡同，除非改 Chromium source** |
| **VLC / MPC-HC / Kodi** | ❌ 預設不會 | 核心 FFmpeg，硬解順序 DXVA2→D3D11VA→Vulkan Video，**不吃 MFT**，失敗就 FFmpeg 軟解。解法：寫 **FFmpeg hwaccel 模組**接 KMDF（不是 MFT）。MPC-HC+LAV 有個少用的 "Media Foundation" 選項 |

> **受眾**：Windows 原生播放器（電影與電視、WMP、TopoEdit）、`IMFMediaEngine` 的 Win32/UWP App。**惠及不到 Chrome 與 VLC。**

## 4. SAND → NV12 放哪
MFT 的 `ProcessOutput` 裡：KMDF 解完 → MFT 拿到 SAND system buffer → **NEON detile 成 linear NV12** → 複製進 `pOutputSamples`。下游 EVR 收到標準 NV12 當一般影片處理（YUV2RGB+繪製），完全不知原本是 SAND。

## 5. 誠實結論 + 里程碑

**誠實結論**：這條讓「Pi5 在 Windows 用原生播放器看本地 4K HEVC」可行；**無法**讓 Chrome 享 YouTube 硬解（架構死穴）。作為 system-level PoC，技術含量極高、能證明 Pi5 VPU 在 Windows 可用。

| M | 目標 | 意義 |
|---|------|------|
| **M1 純 console（脫離 MF）** | 讀 `test.h265` → KMDF → SAND → C 轉 NV12 → `out.yuv` | 驗 KMDF 穩定 + DPB reorder + SAND2NV12 正確（沒通寫 MFT 只會黑畫面無法 debug）|
| **M2 NEON SAND→NV12** | 4K 一幀轉換 <5-10ms | 純 C 會 CPU 滿載掉幀 |
| **M3 Sync MFT + TopoEdit** | 寫 `IMFTransform` 編 `.dll` 註冊；TopoEdit 拉 File Source→HEVC Splitter→你的 MFT→EVR | 避開播放器複雜邏輯，純驗 MF pipeline——**螢幕看到硬解畫面** |
| **M4 征服「電影與電視」** | 移除系統 HEVC 擴充，點 `.mkv`(HEVC) 自動叫起電影與電視流暢播 | 處理 seek/flush/（HVCC vs AnnexB 相容）|

> 下一步：先攻 M1；再找微軟 MFT decoder 範本，把 KMDF 呼叫塞進它的 `ProcessInput`/`ProcessOutput`。
