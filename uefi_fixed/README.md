# `uefi_fixed/` — 我們改過的 Pi5 UEFI source（overlay 上游）

> 目的：在社群 **worproject** 的 Pi5 EDK2 UEFI 上，加入**我們驅動需要的 ACPI 裝置描述**（RP1 周邊），
> rebuild 成自己的 `RPI_EFI.fd`，丟 SD 卡取代原版 → Windows 開機就能用標準 ACPI 列舉 RP1 周邊、載我們的驅動。

## 三個目錄的分工
| 目錄 | 內容 | 版控 |
|------|------|------|
| `uefi_sources/` | 上游原封 source（`edk2` + `edk2-platforms` + `edk2-non-osi`）| ❌ 不納版控（大、各自 `.git`；見下方重建）|
| **`uefi_fixed/`** | **只放我們改/加的檔**（鏡像上游路徑）| ✅ 納版控 |
| `uefi_build/` | build 產出（`RPI_EFI.fd` 等），丟 SD 卡用 | ⚙️ 二進位不版控，保留說明 |

## 結構（按組件鏡像上游，build-uefi.sh 逐一 overlay）
`uefi_fixed/<component>/...` 會覆蓋到 `~/rpi5-uefi/<component>/...`：
- `edk2-platforms/Silicon/RaspberryPi/RpiSiliconPkg/Include/Rp1.asi` — RP1 周邊 ACPI。
- `edk2-platforms/Platform/RaspberryPi/RPi5/AcpiTables/Dsdt.asl` — V3D GPU ACPI 節點（`\_SB.GPU0`）。
- `edk2-platforms/Platform/RaspberryPi/RPi5/Library/PlatformLib/RaspberryPiMem.c` — 16GB RAM 旋鈕。
- `edk2-non-osi/Platform/RaspberryPi/Drivers/LogoDxe/Logo.bmp` — 我們的開機 logo。

## 我們改了什麼（目前）
- **ACPI（`Rp1.asi` + `Dsdt.asl`）**：上游只描述 RP1 底下的 **USB xHCI**。我們**新增** RP1 全周邊
  （**GPIO/I2C/SPI/UART/PWM/I2S/ADC/Ethernet/CLK/PIO/SD**）+ **V3D GPU**（`Dsdt.asl` 的 `\_SB.GPU0`）的
  ACPI `Device` 節點，沿用上游 `PBAR + RP1_*_BASE` + 共享中斷機制。
  - `_HID` 用 **`RPIF000n`**（ACPI 規定後綴須 hex；V3D=`RPIF000D`），class driver INF 已 match `ACPI\RPIF000n`。
  - 細節與決策見 `MD/Note/20260625-2230-pi5-uefi-acpi-integration.md`。
- **16GB RAM 旋鈕（`RaspberryPiMem.c`）**：16GB Pi5 在 Win11-ARM 會卡 logo（見 `MD/HANDOFF.md` §7）。
  `RPI_RAM_CAP_GB`＝**0（預設＝完整 16GB）**；設 8 可壓到 8GB 繞過開機問題（診斷/暫時用）。
- **開機 logo（`Logo.bmp`）**：在原 Raspberry Pi logo 底部加上「**AprPI5WinDriver**」。
  - **維持 8-bit 索引、同尺寸（381×479）、同位元組大小（185012）** → EDK2 LogoDxe 直接相容（同尺寸最保險）。

> **FV 容量真相**：`[FV.FvMain]` 無 `Size=`，**按內容自動長大**（build 報告的「100% / 0 free」只是對齊餘數，不是上限）。
> 真正的牆是它 LZMA 壓進的 `FVMAIN_COMPACT`（1.75MB flash 區，目前 **69% 滿、約 526KB free**）。ACPI 超好壓——實測加 21 個 RP1 節點 raw +4KB、壓縮後僅 +712B。故大量補 ACPI 節點無虞。
  - 改圖工具：`edit-logo.py`（Pillow；只改像素、保留 header/palette/尺寸）。重產：
    `python3 edit-logo.py <原始 Logo.bmp> <輸出 Logo.bmp> "你要的文字"`
    （文字為第 3 參數，省略則用預設；原圖取 pristine：`git -C ~/rpi5-uefi/edk2-non-osi show HEAD:Platform/RaspberryPi/Drivers/LogoDxe/Logo.bmp`；需 `pip install --user --break-system-packages Pillow`）。

> 此法＝「做法 A」（韌體 ACPI 描述）。另一條「做法 B」是 `windows_sources/pcie-rp1/rp1bus`（純後裝、零韌體），
> 兩者擇一或並存；中斷 demux 兩者都仍需驅動處理。

## 如何 build（WSL Ubuntu）— ✅ 已實測成功
> **原版（未改）的完整 build 配方 + 所有踩過的坑 → `MD/Skill/pi5-uefi-build.md`（權威，已驗證 2MB RPI_EFI.fd / 1m32s）。**
> 重點：build harness 是 **`worproject/rpi5-uefi`**（用 worproject fork 的 edk2/edk2-platforms，**非** tianocore master），
> 放 **WSL 原生 FS `~/rpi5-uefi`**（不要 /mnt/c），toolchain tag 是 **`GCC`**（非 GCC5）。

build 我們的修改版：
1. 先照 `MD/Skill/pi5-uefi-build.md` 把 `~/rpi5-uefi` 建好（submodule 全齊、能 build 出原版）。
2. 跑 `wsl bash /mnt/c/ai_project/AprWindowsDriver/uefi_fixed/build-uefi.sh`，它會：
   - 把 `uefi_fixed/` 的檔 overlay 到 `~/rpi5-uefi/edk2-platforms/` 對應路徑（即蓋掉原版 `Rp1.asi`）。
   - 跑 worproject `build.sh --model 5`（`-a AARCH64 -t GCC -b RELEASE`）。
   - 把 `RPI_EFI.fd` 複製到 `uefi_build/`。
3. 把 `uefi_build/RPI_EFI.fd` 丟 SD 卡取代原檔（見 `uefi_build/README.md`）。

## uefi_sources 重建（精簡版——只抓 Pi5 用得到的）
> 全抓三個 repo ≈ **1.1 GB**（90% 是別家板子 + 用不到的 submodule）。RPi5 在 edk2-platforms
> **只引用 3 個子樹**（`Platform/RaspberryPi`、`Silicon/Broadcom`、`Silicon/RaspberryPi`）。

```bash
cd uefi_sources

# edk2-platforms：sparse-checkout 只留 Pi5 三子樹（8311→253 檔，63MB→~2MB）
git clone --depth 1 --no-checkout https://github.com/worproject/edk2-platforms.git
cd edk2-platforms
git sparse-checkout init --cone
git sparse-checkout set Platform/RaspberryPi Silicon/Broadcom Silicon/RaspberryPi
git checkout
cd ..

# edk2：淺 clone，**不要** --recurse-submodules（測試 submodule 用不到）。
# build 若回報缺某 submodule（通常是 CryptoPkg 的 openssl、BaseTools/MdeModulePkg 的 brotli）再單獨 init：
git clone --depth 1 https://github.com/tianocore/edk2.git
#   例：cd edk2 && git submodule update --init --depth 1 \
#       CryptoPkg/Library/OpensslLib/openssl \
#       MdeModulePkg/Library/BrotliCustomDecompressLib/brotli \
#       BaseTools/Source/C/BrotliCompress/brotli

# edk2-non-osi：淺 clone（32MB，含 Pi 用的二進位/logo）
git clone --depth 1 https://github.com/tianocore/edk2-non-osi.git
```
（另需 TF-A 才能組完整可開機映像；只更新 `RPI_EFI.fd` 通常沿用原 SD 卡其餘檔即可。）

> ⚠️ Build 環境：worproject `RPi5.dsc` 預設 **GCC5（Linux）**。本機 **WSL2 目前起不來**
> （`HCS_E_HYPERV_NOT_INSTALLED`——需先 `wsl --install --no-distribution` 裝「虛擬機器平台」+ BIOS 開硬體虛擬化 + 重開機）。
> 開好後跑 `wsl bash uefi_fixed/build-uefi.sh` 即可。
