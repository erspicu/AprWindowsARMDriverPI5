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

## uefi_sources 重建（若尚未備齊）
```bash
cd uefi_sources
git clone --depth 1 https://github.com/worproject/edk2-platforms.git
git clone --depth 1 --recurse-submodules https://github.com/tianocore/edk2.git
git clone --depth 1 https://github.com/tianocore/edk2-non-osi.git
```
（另需 TF-A 才能組完整可開機映像；只更新 `RPI_EFI.fd` 通常沿用原 SD 卡其餘檔即可。）
