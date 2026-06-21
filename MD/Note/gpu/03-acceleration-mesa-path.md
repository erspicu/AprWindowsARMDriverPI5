# V3D GPU 移植：Phase 2/3 加速 — Mesa 重用路線

> 承 [`01`](01-strategy-and-references.md)。加速 = 重用 Mesa（shader 編譯 + CL 產生）+ 自寫 Winsys + KMD。
> Phase 3 完整加速 + 玩家 DXVK 路線見 [`04`](04-full-acceleration-and-knowledge-boundary.md)。

## 1. 在 Windows build Mesa v3d

- **工具鏈：用 `clang-cl`**（非純 MSVC）——Mesa 大量 C99/C11 + GNU extension，clang-cl 偽裝成 MSVC 又相容 Windows SDK。ARM64 build 可行。
- **meson**：`meson setup build --cross-file=arm64_windows.txt -Dos=windows -Dgallium-drivers=v3d -Dgallium-d3d10umd=true -Dshared-glapi=false -Dllvm=disabled`（`gallium-d3d10umd` 名稱依版本，**查 `meson_options.txt`**）。
- **會卡在哪**：
  1. **libdrm 依賴**（`xf86drm.h`/`v3d_drm.h`）→ Windows 沒有；寫 stub header 或把 `v3d_drm.h` 的 ioctl struct 複製給你的 winsys 參考。
  2. **POSIX/fd**：V3D 偷用 Linux fd（`dup`/`mmap`）→ Windows 用 `HANDLE`/`D3DKMT_HANDLE`，把傳 DRM fd 的邏輯換掉。
  3. **V3D simulator**（`v3d_simulator.c` 依賴 Linux）→ `-Dv3d-simulator=disabled`。

## 2. ★ Winsys 介面（最關鍵）

新增 `src/gallium/winsys/v3d/win/`，對照 `v3d_drm_winsys.c` 實作這些（上層 Gallium 用），各對應 WDDM D3DKMT：

| Winsys 介面 | Linux | Windows 實作 |
|------------|-------|--------------|
| `alloc_bo(size,…)` | `DRM_IOCTL_V3D_CREATE_BO` | `D3DKMTCreateAllocation`（填 `D3DDDI_ALLOCATIONINFO`，KMD 配實體記憶體回 `hAllocation`）|
| `bo_map(...)` | mmap | `D3DKMTLock2` / `D3DKMTMapGpuVirtualAddress`（映射到 CPU UM VA，讓 Mesa 寫 uniform/CL）|
| `submit_cl(...)` | `DRM_IOCTL_V3D_SUBMIT_CL` | `D3DKMTSubmitCommand` / `D3DKMTRender`（把 binner/render CL 的 GPU VA+長度封進 private driver data 給 KMD）|
| `bo_wait(...)` | `DRM_IOCTL_V3D_WAIT_BO` | monitored fence + `D3DKMTWaitForSynchronizationObjectFromCpu` |
| `get_param(...)` | `DRM_IOCTL_V3D_GET_PARAM` | `D3DKMTEscape`（後門查 V3D 版本/QPU 數/記憶體）|

KMD 端對應：`CREATE_BO`→`DxgkDdiCreateAllocation`（設頁表）、`SUBMIT_CL`→`DxgkDdiSubmitCommand`（驗證 + 寫 V3D CLE 暫存器觸發）、`WAIT_BO`→fence + `DxgkDdiInterruptRoutine`。

## 3. d3d10umd state tracker

- Windows DX runtime（`d3d11.dll`）載入你的 UMD DLL；Mesa `d3d10umd` 實作 `OpenAdapter10_2`/`CreateDevice` 等 DDI，轉成 Gallium `pipe_context`（`draw_vbo`/`set_constant_buffer`）。
- **KMD 提供什麼**：UMD 初始化時 DX runtime 傳入 callbacks（`pfnAllocateCb`/`pfnEscapeCb`/`pfnRenderCb`）——你的 winsys **用這些 callback**跟 KMD 溝通（不是自己 LoadLibrary 叫 D3DKMT）。
- **DWM 需求**：要 D3D11 FL 11_0/10_0；V3D 7.1（支援 Vulkan 1.2/GLES 3.1）能力足夠，`d3d10umd` 支援 D3D11 DDI。**需實機驗證 DWM 對特定 format 的硬性要求**。

## 4. ★ Phase 2 保命路線（不掛 WDDM DWM，先驗 GPU 真的會跑）

> 一開始就寫完整 Dxgkrnl miniport 會 BSOD 到崩潰。先繞過 WDDM。

- **極簡 KMD**：標準 **KMDF**（非 display miniport），建 `\\.\V3D_Pi5`，`EvtIoDeviceControl` 開自訂 IOCTL：`ALLOC_MEM` / `MAP_MEM` / `SUBMIT_CL` / `WAIT_IDLE`。
- **改 Mesa winsys**：不叫 D3DKMT，改用 `CreateFile("\\\\.\\V3D_Pi5")` + `DeviceIoControl(IOCTL_V3D_SUBMIT_CL,…)`。
- **跟完整 WDDM 差在**：不歸 Dxgkrnl 管、沒 VidMm/VidSch、要自己把實體記憶體映進 V3D MMU、不能註冊成 display miniport。
- **第一個「GPU 真的有跑」驗收（不要先跑 shader）**：
  > **硬體 clear 一塊 buffer** — 手刻最短 CL（設 tile binning + V3D 硬體 clear 指令填 `0xDEADBEEF` 紅），submit，等 IRQ/polling，CPU 讀回看是不是變紅。**做到這點 = PCIe/SoC 暫存器映射 + GPU MMU 頁表 + 中斷 + clock/power 全通了**（最難的一步）。

## 5. Shader 編譯鏈（OS 無關）

`DXBC`（D3D 應用）→ Mesa `d3d10umd` frontend → `TGSI`/`NIR` → V3D 編譯器 `src/broadcom/compiler/v3d_compiler.c` → **V3D QPU assembly** → 塞進 shader buffer 經 CL 上傳。
- DXBC→NIR：Mesa 內成熟。NIR→QPU：純 CPU、與 OS 無關，只要能在 Windows 編出 Broadcom compiler + NIR 模組即可。
- **坑**：V3D 編譯器吃重 NIR 優化；確保 clang-cl 的 `-fno-math-errno`/`-fno-strict-aliasing` 等有正確透傳，否則浮點行為異常會產壞 QPU code。
