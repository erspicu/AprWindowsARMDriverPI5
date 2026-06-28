# 專案交接 + 總說明（HANDOFF）

> **給接手者（新 session / 另一台機 / Win11-ARM 端的 Claude / 人）的「從這裡開始」文件。**
> 讀完這份 + `CLAUDE.md`（每 session 自動載入）+ `MD/Note/RPi5-Porting-Status.md`（單一進度真相），即可無縫接手。
> 最後更新：2026-06-28。

---

## 1. 專案一句話
把 **Raspberry Pi 5**（BCM2712 SoC + RP1 PCIe I/O 南橋）的 **Linux 驅動**移植到 **Windows on ARM64**。
本質是**重寫**（非重編譯）：沿用暫存器定義/初始化序列/演算法；重寫 OS 介接層（記憶體/中斷/DMA/電源/bus 列舉）；硬體描述 **Device Tree → ACPI**。

## 2. 雙機交接模型（最重要的前提）
| 機器 | 角色 | 能做到 |
|------|------|--------|
| **Zen2 x64（本機，開發機）** | cross-compile ARM64、補邏輯、**x64 sim 驗證**、寫 ACPI/INF、build UEFI、對 Linux 源碼比對 | 驅動 → 🔵（邏輯完整+sim）；純軟體交付物(ACPI/INF/UEFI) → ✅ |
| **Pi5（實機）** | 跑 Win11-ARM、刷韌體、載驅動、KDNET 雙機 WinDbg、校時序/中斷/DMA/BSOD 除錯 | 🔵 → ✅（功能驗證） |
| **Pi5（跑 Linux，唯讀基準機）** | SSH **唯讀**萃取硬體真相（暫存器值/IRQ/記憶體 map/vulkaninfo）校正驅動 | 提供校正資料，**不是**載入/測試目標 |

> ⚠️ Pi5 Linux SSH **全程唯讀**：不 devmem 寫、不讀未供電周邊（bus-error）、不 unbind eth0(SSH 命脈)/mmc(rootfs)。連線法見 `MD/Skill/pi5-ssh-hardware-facts.md`。

## 3. 現況總覽（2026-06-28）
- **驅動**：~25 個裝置驅動，絕大多數推到 **🔵**（HAL 邏輯 + x64 sim 驗證 + ARM64 /kernel 編譯乾淨 + Pi5 源碼/實機校正）。詳見 `RPi5-Porting-Status.md`。
- **韌體（UEFI）✅ 可 build**：客製 worproject Pi5 UEFI，加入 RP1 全周邊 + V3D 的 ACPI 節點 + 自訂 logo，WSL 實測編出 `RPI_EFI.fd`。
- **GPU**：V3D WDDM render KMD 可列舉空殼 + Mesa v3dv 的 Windows winsys 後端，4 個純邏輯模組 sim 驗證。
- **仍待**：**全部需 Pi5 Win11-ARM 實機**做功能驗證（載入/中斷/DMA/fence/BSOD 除錯）。x64 端能榨的純邏輯基本到頂。

## 4. 倉庫結構（哪些納版控）
| 目錄 | 內容 | 版控 |
|------|------|------|
| `windows_sources/` | 驅動原始碼（HAL+sim+driver+build.ps1），含 `gpu/rp1-v3d`(KMD)、`gpu/v3dv-wddm`(UMD)、`pcie-rp1/rp1bus`(備案 bus driver) | ✅ |
| `windows_driver/` | INF（infverif 過）；`.sys` 為 build 產出 | INF✅ / .sys 部分 |
| `uefi_fixed/` | **我們改的 UEFI source**（overlay 上游）：`Rp1.asi`(RP1+V3D ACPI)、`Dsdt.asl`(V3D 節點)、`RaspberryPiMem.c`(16GB RAM 旋鈕)、`Logo.bmp`、`build-uefi.sh`、`edit-logo.py` | ✅ |
| `uefi_build/` | `RPI_EFI.fd`（刷 SD 卡） | 二進位 -f 追蹤 |
| `uefi_sources_backup/` | **30MB 可編譯 UEFI 凍結備份**（端到端驗證，防 fork/鏡像消失） | ✅ |
| `MD/` | 文件：`Note/`(know-how+狀態)、`Skill/`(how-to)、`teachbook/`、`History/` | ✅ |
| `tools/knowledgebase/` | Gemini 查詢腳本（金鑰 `C:\key\config.json`，不在 repo） | ✅ |
| `sources/` | Pi Linux 核心 + Infineon WHD（移植參考） | ❌ 重建→`Skill/sources-rebuild.md` |
| `uefi_sources/` | 上游 EDK2/worproject UEFI 樹（build harness `~/rpi5-uefi`） | ❌ 重建→`Skill/pi5-uefi-build.md` |
| `gpu_driver_sources/` | KMDOD/viogpu/Mesa 參考源 | ❌ 重建→`Skill/gpu-driver-sources-rebuild.md` |
| `temp/` `private/` | 暫存/個人 | ❌ |

> **未納版控的大目錄都有對應的 `MD/Skill/*-rebuild.md`**（blobless+sparse clone 指令、pin commit、驗證）。換機照做即可重建。

## 5. 關鍵決策（別再糾結這些）
1. **GPU/RP1 周邊列舉 = 做法 A（客製 UEFI ACPI）為主線**：Pi5 跑 Windows 本就得刷客製 UEFI，把 ACPI 塞進去零額外成本、走標準框架。`rp1bus.sys`(做法 B，純後裝 bus driver) 降為備案。見 `Note/20260625-2230-...`。
2. **GPU 3D = Vulkan-first**：寫一個 WDDM render KMD + port Mesa **v3dv**(Vulkan) 的 Windows winsys，上層 **Zink(OpenGL)/DXVK·vkd3d(D3D)/clvk(OpenCL bonus)** 全解鎖。不自寫 D3D UMD。見 `Note/20260626-0200`。
3. **藍牙 = inbox BthUart.sys**（`bthx.h` 不在現代公開 WDK）；我們只做 BCM `.hcd` + baud 薄處理。
4. **WiFi = WHD + NetAdapterCx**（偽裝乙太網，不寫 dot11/WDI）。
5. **驅動不進 UEFI**：ACPI(裝置描述)進韌體；驅動 `.sys` 用 **DISM 注入 Windows 映像**。見 `Note/20260626-0130-...packaging-sop`。

## 6. 開發方法/慣例
- **x64 sim pattern**：每個 register-HAL 抽成純 `void*/PUCHAR Base` 邏輯 + `sim/*_simshim.h`(typedef+SAL stub) + `sim/*_sim.c`(mock 暫存器+斷言)。`cl /D<NAME>_SIM /I.. sim.c hal.c` 編譯跑驗證。**不需硬體**。每個 HAL 也 ARM64 `/kernel` 編譯檢查。
- **clang IDE 紅字（ntddk.h not found / Unknown ULONG）是恆定假陽性**（clang 沒給 WDK include / SIM define）。**以 cl.exe 實際編譯 + sim 跑過為準。**
- **build**：各驅動 `windows_sources/<類別>/<專案>/build.ps1`（直呼 cl/link，KMDF/GpioClx/dispmprt 視類別）。WDK 10.0.26100 + VS2022 ARM64。
- **UEFI build**：`wsl bash uefi_fixed/build-uefi.sh`（overlay 我們的檔→`~/rpi5-uefi`→build.sh→`uefi_build/RPI_EFI.fd`）。配方/踩坑 `Skill/pi5-uefi-build.md`。
- **單一進度真相 = `Note/RPi5-Porting-Status.md`**：每完成階段性產出就更新；同時新增 `Note/年月日-時間-主題.md` 記 know-how。
- **commit**：結尾加 `Co-Authored-By: Claude Opus 4.8 ...` + `Claude-Session: ...`。GitHub repo **Private**（github.com/erspicu/AprWindowsARMDriverPI5）。
- **諮詢**：`python tools/knowledgebase/gemini_query.py -f <prompt> -o <out>`（Gemini 給方向，**細節對真 header/源碼查證**——sdbus/bthx 都是 Gemini 給過時答案、以真 header 為準的教訓）。

## 7. UEFI 韌體線（含 16GB 問題）
- **build harness**：`worproject/rpi5-uefi`（**fork**：edk2 `sdmmc-dev`、edk2-platforms `rpi5-dev`、TF-A `rpi5`，**非** tianocore master），WSL2 build，toolchain tag `GCC`（非 GCC5）。踩坑全記在 `Skill/pi5-uefi-build.md`（subhook 用 `tianocore/edk2-subhook` 鏡像、`GIT_TERMINAL_PROMPT=0` 防 auth hang、python 別名…）。
- **我們的 ACPI**：`uefi_fixed/.../Rp1.asi`（RP1 周邊 GPIO/I2C/SPI/UART/PWM/I2S/ADC/ETH/CLK/PIO/SD，`_HID=RPIF000n`，後綴須 hex）+ `Dsdt.asl`（V3D `\_SB.GPU0`/`RPIF000D`/3 MMIO+IRQ 282/281）。FvMain 自動長大、真正餘裕在 FVMAIN_COMPACT(~526KB)。
- **驅動 INF 對齊**：`windows_driver/*/*.inf` 的 HWID 已對齊 `ACPI\RPIF000n`。
- **🔴 16GB Pi5 開機問題（未解，進行中）**：16GB 板 Win11-ARM **卡在 UEFI logo 後、無 spinner、進不去**。
  - 已知：Pi5 revision `e04171`→解碼 16GB（正確）；`config.txt total_mem` **無效**（UEFI 用 revision code 算大小，不看 mailbox）；UEFI 的 >4GB 記憶體 map 碼看起來對 → 崩在下游(ACPI/TF-A/kernel)。真實 16GB 佈局：System RAM `0x80000-0x3ffffffff`。
  - 旋鈕：`uefi_fixed/.../RaspberryPiMem.c` 的 `RPI_RAM_CAP_GB`（0=完整16GB，預設；設 8=壓 8GB 可能繞過開機）。
  - **下一步（需實機）**：接 UART 序列埠看 Windows loader 死在哪、或 SD 卡 BCD `bcdedit /set {default} sos yes` 看停在哪個 `.sys`（常是 acpi.sys 附近）→ 定位是 ACPI 記憶體描述 / TF-A DRAM / kernel 限制，再對症修。

## 8. GPU 線（V3D，最大的一塊）
- **策略堆疊**：KMD + v3dv(Vulkan) → Zink/DXVK/vkd3d/clvk。**Windows 上躲不掉 WDDM KMD**（Vulkan/GL 也得經 dxgkrnl，不能像 Linux 直打 DRM）。
- **KMD（`gpu/rp1-v3d`）**：DxgkInitialize render-only 空殼——StartDevice 映射 V3D 3 區塊(hub/core0/sms 分開)+讀 IDENT、QueryAdapterInfo DRIVERCAPS、BuildPagingBuffer(FLUSH_TLB)、SubmitCommand(讀命令私有資料寫 CT0/1 觸發)。
- **UMD（`gpu/v3dv-wddm`）**：v3dv 的 `v3d_ioctl` 咽喉點加第三分支 → `v3d_wddm_ioctl()` 把 DRM_IOCTL_V3D_* 翻 D3DKMT（比照 v3d_simulator 模型）。
- **純邏輯模組（全 sim 驗證）**：`v3d_pte`(PTE encoder 10/10)、`v3d_engine`(CL submit+MMU config 11/11，`MMU_CTL==0x060D0C01` 對上實機)、`v3dv_wddm`(ioctl 翻譯 18/18)。
- **Pi5 實機校正**：V3D 7.1.10.16、IDENT 值、TFU:no、中斷 GSIV 281(core0)/282(hub)、vulkaninfo（`Note/20260626-0330`）。
- **🔴 待實機**：D3DKMT 實接(open adapter/alloc/submit/sync)、`BuildPagingBuffer` 的 DXGK page 迴圈、monitored fence、BSOD 除錯。再來才是 v3dv build/Zink/DXVK。
- 文件：`Note/20260626-0200`(策略)/`0230`(KMD 參考)/`0300`(UMD port 面)/`0330`(V3D facts)；`gpu/README.md`(早期研究)。

## 9. 接 Pi5 Win11-ARM 實機的第一步
**先決閘門**：Windows 能在 Pi5 開機到桌面（刷我們的 `RPI_EFI.fd`；先解 16GB 開機問題見 §7）。然後照 **`Skill/pi5-win11-bringup-playbook.md`**：
1. 環境（testsigning、crash dump、KDNET、救援後路）。
2. PCIe/RP1 列舉（命脈）。
3. 標準周邊（GPIO/I2C/SPI/UART/PWM/ADC/RNG/watchdog/DMA/mailbox/I2S/SDHCI/Ethernet…）逐一 `pnputil` 載入→IOCTL 測→收 log→修。每個都有第一驗收點 + 對應已寫好的 HAL/sim。
4. 顯示 DOD、無線(BT/WiFi)、多媒體、最後 GPU KMD。

## 10. 已知未解 / TODO（依優先序）
1. **16GB UEFI 開機**（§7）— 需序列埠 log 定位。
2. **接實機把 🔵 推到 ✅**（playbook）。
3. **GPU 深層核心**（D3DKMT 實接 + fence + render）。
4. 大工程：PiSP 硬體 ISP、VCHIQ、DSI/VEC 顯示輸出（只有筆記）。
5. 封裝：DISM 注入驅動 + 簽章（`Note/20260626-0130`）。

## 11. 新 session「從這裡開始」
1. clone repo → `CLAUDE.md` 自動載入（專案守則 + 路徑引導）。
2. 讀本檔（HANDOFF）+ `Note/RPi5-Porting-Status.md`（現況真相）。
3. 要動韌體/GPU/特定裝置 → 讀對應 `Note/` 專題 + `Skill/` how-to。
4. 未納版控的 `sources/`/`uefi_sources/`/`gpu_driver_sources/` → 照 `Skill/*-rebuild.md` 重建。
5. 慣例見 §6（sim pattern、clang 假陽性、commit 格式、Gemini 查證原則）。
6. **記憶**：本機 file-based memory 在 `~/.claude/.../memory/`（不跨機；重要事實多已落在上述文件）。
