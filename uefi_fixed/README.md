# `uefi_fixed/` — 我們改過的 Pi5 UEFI source（overlay 上游）

> 目的：在社群 **worproject** 的 Pi5 EDK2 UEFI 上，加入**我們驅動需要的 ACPI 裝置描述**（RP1 周邊），
> rebuild 成自己的 `RPI_EFI.fd`，丟 SD 卡取代原版 → Windows 開機就能用標準 ACPI 列舉 RP1 周邊、載我們的驅動。

## 三個目錄的分工
| 目錄 | 內容 | 版控 |
|------|------|------|
| `uefi_sources/` | 上游原封 source（`edk2` + `edk2-platforms` + `edk2-non-osi`）| ❌ 不納版控（大、各自 `.git`；見下方重建）|
| **`uefi_fixed/`** | **只放我們改/加的檔**（鏡像上游路徑）| ✅ 納版控 |
| `uefi_build/` | build 產出（`RPI_EFI.fd` 等），丟 SD 卡用 | ⚙️ 二進位不版控，保留說明 |

## 我們改了什麼（目前）
- `Silicon/RaspberryPi/RpiSiliconPkg/Include/Rp1.asi`
  - 上游只描述 RP1 底下的 **USB xHCI**（所以 stock Win11 只有鍵鼠/USB 能動）。
  - 我們**新增** RP1 的 **GPIO / I2C / SPI / UART / PWM / I2S / ADC / Ethernet** 的 ACPI `Device` 節點，
    沿用上游 `RP1_QWORDMEMORY_SET`（`PBAR + RP1_*_BASE`）+ 共享中斷 `PINT` 機制。
  - `_HID` 用 `RPI0xxxx`，我們的 class driver INF 要 match `ACPI\RPI0xxxx`。
  - 細節與決策見 `MD/Note/20260625-2230-pi5-uefi-acpi-integration.md`。

> 此法＝「做法 A」（韌體 ACPI 描述）。另一條「做法 B」是 `windows_sources/pcie-rp1/rp1bus`（純後裝、零韌體），
> 兩者擇一或並存；中斷 demux 兩者都仍需驅動處理。

## 如何 build（WSL Ubuntu）
1. 先確保 `uefi_sources/` 已備齊三個 repo（見該目錄 `HOWTO-REBUILD.md` 或下方）。
2. 跑 `bash uefi_fixed/build-uefi.sh`，它會：
   - 把 `uefi_fixed/` 的檔**覆蓋**到 `uefi_sources/edk2-platforms/` 對應路徑（overlay）。
   - 設定 EDK2 workspace、build BaseTools、`build -a AARCH64 -t GCC5 -p .../RPi5/RPi5.dsc -b RELEASE`。
   - 把 `Build/RPi5/RELEASE_GCC5/FV/RPI_EFI.fd` 複製到 `uefi_build/`。
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
