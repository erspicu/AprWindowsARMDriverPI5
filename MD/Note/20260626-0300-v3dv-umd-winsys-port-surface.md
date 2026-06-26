# v3dv UMD（Vulkan）winsys port 面分析（2026-06-26）

> 研究 Mesa v3dv（`gpu_driver_sources/mesa/src/broadcom/vulkan/`）的核心介面，定位「Linux DRM → Windows D3DKMT」要改哪裡。
> 策略總綱 `20260626-0200`（Vulkan-first）；KMD 端 `20260626-0230` + `windows_sources/gpu/rp1-v3d`。

## 🎯 最關鍵發現：單一咽喉點 + 已有抽象先例
v3dv 對核心的**所有**呼叫都過一個 inline wrapper（`src/broadcom/common/v3d_util.h:115`）：
```c
static inline int v3d_ioctl(int fd, unsigned long request, void *arg) {
#if USE_V3D_SIMULATOR
        return v3d_simulator_ioctl(fd, request, arg);   // 無硬體模擬器（已存在）
#else
        return drmIoctl(fd, request, arg);              // Linux DRM
#endif
}
```
→ **Windows port = 加第三分支** `#elif USE_V3D_WDDM → v3d_wddm_ioctl(fd, request, arg)`，把每個 `DRM_IOCTL_V3D_*` 翻成 D3DKMT。
**simulator 證明這個攔截模型可行**（它就是攔同一批 request 來模擬）——我們照抄結構，目標換成「呼叫我們的 KMD」。

## 核心介面全集（要在 v3d_wddm_ioctl 翻譯的 request）
| DRM ioctl | 在哪 | 功能 | → D3DKMT 對應 | 對應我們 KMD DDI |
|-----------|------|------|---------------|------------------|
| `CREATE_BO` | v3dv_bo.c | 配 GPU buffer | `D3DKMTCreateAllocation` | `DxgkDdiCreateAllocation` |
| `MMAP_BO` | v3dv_bo.c | CPU map BO | `D3DKMTLock2` / `Lock` | (VidMm) |
| `GET_BO_OFFSET` | v3dv_bo.c | 取 GPU VA | `D3DKMTMapGpuVirtualAddress` 的 VA | `BuildPagingBuffer`(page table) |
| `WAIT_BO` | v3dv_bo.c | 等 BO 閒置 | `D3DKMTWaitForSynchronizationObject` | fence |
| `GEM_CLOSE` | v3dv_bo.c | 釋放 BO | `D3DKMTDestroyAllocation` | `DxgkDdiDestroyAllocation` |
| `SUBMIT_CL` | v3dv_queue.c | 提交 binner+render job | `D3DKMTSubmitCommand`/`...ToHwQueue` | `DxgkDdiSubmitCommandVirtual`→寫 CT0/1 CA/EA |
| `SUBMIT_CSD` | v3dv_queue.c | 提交 compute job | 同上 | 同上 |
| `SUBMIT_TFU` | v3dv_queue.c | texture format unit（blit）| 同上 | 同上 |
| `SUBMIT_CPU` | v3dv_queue.c | CPU 佇列（query 等）| usermode 處理或 D3DKMT | — |
| `GET_PARAM` | v3dv_device.c | 查 V3D 能力 | 靜態回（我們已知 V3D 7.1）或 `D3DKMTQueryAdapterInfo` private | `QueryAdapterInfo` |
| `PERFMON_*` | v3dv_query.c | 效能計數器 | **可先略過**（非必須）| — |
| `drmSyncobj*` | v3dv_queue.c | fence/同步物件 | `D3DKMTCreateSynchronizationObject2` + `Submit/WaitForSyncObject...` | monitored fence |

## 4 個 port 工作面
1. **`v3d_ioctl` 翻譯層**（核心）：新增 `v3d_wddm_ioctl()`，switch on `request` → 上表 D3DKMT。**這是 80% 的工**，但因為集中在一個 switch，邊界清楚。
2. **裝置開啟**：v3dv_device.c 用 `render_fd`（DRM render node int fd）。Windows 改成 `D3DKMTOpenAdapterFromLuid` + `D3DKMTCreateDevice` → 把回傳 handle 塞進 `fd` 位置（或改型別/查表）。`v3d_simulator_init(render_fd)` 對應點 = `v3d_wddm_init()`。
3. **fence / 同步**：`drmSyncobj*`（4 處）→ `D3DKMTCreateSynchronizationObject2` 系列（monitored fence）。
4. **BO ↔ allocation + GPU VA**：BO 的 handle/offset 語意對到 D3DKMT allocation handle + GpuVirtualAddress；GPU VA 由 WDDM GpuMmu 管（我們 KMD 的 `BuildPagingBuffer` 寫 V3D PTE）。

## 要改的檔（集中、不擴散）
- `src/broadcom/common/v3d_util.h` — 加 WDDM 分支。
- `src/broadcom/vulkan/v3dv_wddm.c`（新）— `v3d_wddm_ioctl/init`：D3DKMT 翻譯。**比照既有 `v3d_simulator.c` 寫**。
- `src/broadcom/vulkan/v3dv_device.c` — 裝置開啟改 D3DKMT。
- `src/broadcom/vulkan/v3dv_bo.c` / `v3dv_queue.c` — `fd` 語意 + syncobj（多數透過 `v3d_ioctl` 自動轉，少數直接 drm 呼叫要改）。
- build：Mesa 用 meson；Windows build v3dv ICD 另需 meson cross + Windows winsys（參考 Mesa Dozen/d3d12 的 Windows build）。

## 為何這條路務實
- **單一咽喉點 + simulator 先例** → port 邊界清楚（不是「重寫 v3dv」，是「加一個 winsys 後端」）。
- v3dv 的硬體知識（CL 封包、tiling、shader 編譯）**完全沿用**，只換「跟核心講話」那層。
- 配合我們的 KMD（CreateAllocation/SubmitCommandVirtual/BuildPagingBuffer/fence）兩端對接。

## 待查證 / 後續
- D3DKMT 從 usermode 的確切 API/header（`d3dkmthk.h`）+ 是否需要 dxcore 開 adapter。
- v3dv 的 syncobj 用法是否全走 `v3d_ioctl`（部分 `drmSyncobj*` 直接呼叫，要逐一改）。
- Mesa 在 Windows build v3dv 的 meson 設定（cross file + winsys）。
- 第一個里程碑：`v3d_wddm_ioctl` 只實作 GET_PARAM + CREATE_BO + MMAP_BO，讓 `vkCreateInstance`/列舉裝置不崩 → 再逐步補 SUBMIT/fence。
