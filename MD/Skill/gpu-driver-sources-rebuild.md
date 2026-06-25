# Skill：重建 `gpu_driver_sources/`（WDDM KMD/UMD 參考源碼）

> `gpu_driver_sources/` 是 **GPU 驅動移植的參考源碼**（WDDM KMD 骨架 + 真實 render KMD + Mesa v3dv/v3d）。
> **不納版控**（`.gitignore`，blobless+sparse 約 **31MB**）；換機/新環境用本檔重建。
> 用途與架構見 `MD/Note/20260626-0230-wddm-kmd-references.md`、策略見 `20260626-0200-pi5-gpu-accel-stack.md`。

## 內容（cone sparse-checkout，只取相關子樹）
| 目錄 | repo / branch | 抓的子樹 | 用途 | 本次 commit |
|------|---------------|---------|------|-------------|
| `Windows-driver-samples/` | `microsoft/Windows-driver-samples` main | `video` | **KMDOD 骨架**（`video/KMDOD`，砍 display 當 render KMD 底；我們 DOD 已用同套）| `26741c7` |
| `kvm-guest-drivers-windows/` | `virtio-win/kvm-guest-drivers-windows` master | `viogpu` | ⭐ **唯一開源真實 render WDDM KMD**（讀 `BuildPagingBuffer`/`SubmitCommand`；含 `viogpudo`/`viogpusc`/`common`/`shared`）| `54c4398` |
| `mesa/` | `mesa/mesa`（gitlab.freedesktop.org）main | `src/broadcom` `src/gallium/drivers/v3d` `src/gallium/winsys/v3d` `src/gallium/winsys/d3d12` `src/vulkan` `src/util` `include` | **v3dv**(Vulkan, `src/broadcom/vulkan/v3dv_*`)、**v3d gallium**(GL)、**d3d12 winsys/wgl**(D3DKMT 範本，給 v3dv winsys port)| `8b28b4d` |

> Linux **kernel** V3D（`drivers/gpu/drm/v3d`）不在這裡——它在 `sources/`（Pi kernel，見 `MD/Skill/sources-rebuild.md`）。

## 重建指令（WSL 或任何 git；blobless + 單一連線防 hang）
```bash
cd C:/ai_project/AprWindowsDriver
mkdir -p gpu_driver_sources && cd gpu_driver_sources
export GIT_TERMINAL_PROMPT=0          # 遇 auth 立即失敗、不要 hang

# 1) KMDOD 骨架
git clone --filter=blob:none --no-checkout --depth 1 \
    https://github.com/microsoft/Windows-driver-samples.git
( cd Windows-driver-samples && git sparse-checkout init --cone \
  && git sparse-checkout set video && git checkout )

# 2) viogpu（真實 render KMD）
git clone --filter=blob:none --no-checkout --depth 1 \
    https://github.com/virtio-win/kvm-guest-drivers-windows.git
( cd kvm-guest-drivers-windows && git sparse-checkout init --cone \
  && git sparse-checkout set viogpu && git checkout )

# 3) Mesa（v3dv/v3d/d3d12 winsys）
git clone --filter=blob:none --no-checkout --depth 1 \
    https://gitlab.freedesktop.org/mesa/mesa.git
( cd mesa && git sparse-checkout init --cone \
  && git sparse-checkout set src/broadcom src/gallium/drivers/v3d \
       src/gallium/winsys/v3d src/gallium/winsys/d3d12 src/vulkan src/util include \
  && git checkout )
```

## 驗證（全 ✓ 才算對）
```bash
cd C:/ai_project/AprWindowsDriver/gpu_driver_sources
[ -f Windows-driver-samples/video/KMDOD/bdd.cxx ]            && echo "✓ KMDOD" || echo "✗ KMDOD"
[ -d kvm-guest-drivers-windows/viogpu/viogpudo ]             && echo "✓ viogpu" || echo "✗ viogpu"
[ -f mesa/src/broadcom/vulkan/v3dv_cmd_buffer.c ]            && echo "✓ v3dv"  || echo "✗ v3dv"
[ -d mesa/src/gallium/winsys/d3d12 ]                         && echo "✓ d3d12 winsys" || echo "✗ d3d12"
```

## 重點檔（攻 KMD 時常看）
| 主題 | 路徑 |
|------|------|
| KMDOD WDF/DxgkDdi 初始化樣板 | `Windows-driver-samples/video/KMDOD/`（`bdd.cxx`/`bdd.hxx`）|
| 真實 render KMD：記憶體/提交 | `kvm-guest-drivers-windows/viogpu/viogpudo/`（找 `BuildPagingBuffer`/`SubmitCommand`/`Present`）|
| v3dv（Vulkan KMD↔UMD 介面、CL 提交）| `mesa/src/broadcom/vulkan/v3dv_{bo,cl,cmd_buffer,device,queue}.c` |
| v3d gallium（GL pipe driver）| `mesa/src/gallium/drivers/v3d/` |
| Linux ioctl ↔ D3DKMT 對應範本 | `mesa/src/gallium/winsys/d3d12/wgl/` |
| Linux kernel V3D（暫存器/提交/MMU 概念）| `sources/drivers/gpu/drm/v3d/`（另一棵樹）|

## 備註
- 三個 repo 各自 `.git`、`--depth 1`（最新）。要逐字重現可改 checkout 上表 commit。
- Mesa 來自 freedesktop GitLab（非 github）；其餘 github。皆匿名可 clone。
- 全部不在本專案版控內（clone 不含，依本檔重建）。
