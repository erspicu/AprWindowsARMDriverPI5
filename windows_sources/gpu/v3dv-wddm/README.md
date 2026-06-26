# v3dv WDDM winsys backend（Mesa v3dv 的 Windows/D3DKMT 後端）

> 把 Mesa v3dv（Vulkan）的核心呼叫從 Linux DRM 轉到 Windows D3DKMT。
> 設計分析見 `MD/Note/20260626-0300-v3dv-umd-winsys-port-surface.md`；策略 `20260626-0200`。

## 檔案
| 檔 | 作用 |
|----|------|
| `v3dv_wddm.c` / `.h` | **翻譯層**：`v3d_wddm_ioctl()` 把 `DRM_IOCTL_V3D_*` 翻成 D3DKMT（D3DKMT 呼叫目前是 stub，標 TODO）|
| `sim/wddm_shim.h` + `sim/wddm_sim.c` | x64 編譯驗證翻譯 switch（**sim 15/15**：GET_PARAM/CREATE_BO+GPU VA/GET_BO_OFFSET/MMAP_BO/SUBMIT_CL/GEM_CLOSE/未知→ENOSYS）|

## 怎麼接進 Mesa（overlay）
1. 複製 `v3dv_wddm.c/.h` 到 `mesa/src/broadcom/vulkan/`。
2. 在 `mesa/src/broadcom/common/v3d_util.h` 的 `v3d_ioctl()` 加第三分支：
   ```c
   static inline int v3d_ioctl(int fd, unsigned long request, void *arg) {
   #if USE_V3D_SIMULATOR
           return v3d_simulator_ioctl(fd, request, arg);
   #elif defined(USE_V3D_WDDM)
           return v3d_wddm_ioctl(fd, request, arg);   /* ← 新增 */
   #else
           return drmIoctl(fd, request, arg);
   #endif
   }
   ```
3. `v3dv_device.c` 開裝置改 `v3d_wddm_init()`（取代 DRM render node open）。
4. meson：加 `USE_V3D_WDDM` build option + 把 `v3dv_wddm.c` 納入 v3dv 的 `meson.build`；Windows cross 設定參考 Mesa Dozen/d3d12。

## 現況 / 下一步
- ✅ 翻譯 switch 結構 + BO table + GET_PARAM/CREATE_BO/MMAP/GET_BO_OFFSET/GEM_CLOSE/SUBMIT 路由（sim 驗證）。
- 🔴 **D3DKMT 實接**（`d3dkmt_*` stub → 真 `D3DKMTCreateAllocation/Lock2/SubmitCommandToHwQueue/...`）：需 `d3dkmthk.h` + 我們的 KMD 在 Pi5 上跑。
- 🔴 **裝置開啟**（`D3DKMTOpenAdapterFromLuid`+`CreateDevice`）、**fence**（`D3DKMTCreateSynchronizationObject2`）。
- 🔴 V3D IDENT（GET_PARAM 的 CORE0_IDENT*）由 KMD `QueryAdapterInfo` private 回傳填入。
- 里程碑：D3DKMT 實接 GET_PARAM+CREATE_BO+MMAP → `vkCreateInstance`/`vkEnumeratePhysicalDevices` 在實機不崩。

## 與 KMD 對接
`v3d_wddm.c` 的每個 D3DKMT 呼叫對應我們 KMD（`windows_sources/gpu/rp1-v3d`）的 DDI：CreateAllocation / BuildPagingBuffer(GPU VA) / SubmitCommandVirtual(CT0/1) / monitored fence。見 port-surface note 的對照表。
