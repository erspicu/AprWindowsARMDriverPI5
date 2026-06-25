# Skill：build Pi5 Win11-ARM UEFI（worproject）— 已驗證可行配方

> **狀態：✅ 實測成功**（2026-06-26）。產出 `RPI_EFI.fd` **2,031,616 bytes**，build 耗時 **1m32s**。
> 這份是「原版（未改）」的正確 build 法 + 我們踩過的所有坑。改版（加我們 ACPI）見 `uefi_fixed/README.md`。

## 0) 結論先講：正確的 source 不是 tianocore master！
- build harness = **`worproject/rpi5-uefi`**，它用 4 個 submodule（**都是 worproject fork 的特定分支**，非 tianocore master）：
  | submodule | repo / branch | 本次 commit |
  |-----------|---------------|-------------|
  | edk2 | `worproject/edk2` `sdmmc-dev` | `eca8aae` |
  | edk2-platforms | `worproject/edk2-platforms` `rpi5-dev` | `8e1779b` |
  | edk2-non-osi | `tianocore/edk2-non-osi` master | `1f4d784` |
  | arm-trusted-firmware | `worproject/arm-trusted-firmware` `rpi5` | `682607f` |
  | (rpi5-uefi 本體) | `worproject/rpi5-uefi` | `a6135b0` |
- ❌ **別**抓 `tianocore/edk2` master：它移除了 `ArmPlatformPkg/PrePi/`，RPi5.dsc 會報 `PeiUniCore.inf not found`。

## 1) 環境
- **WSL2**（Ubuntu 24.04）。WSL2 需 BIOS 開硬體虛擬化（AMD **SVM** / Intel **VT-x**）+ Windows「虛擬機器平台」功能；否則 `wsl` 起不來報 `HCS_E_HYPERV_NOT_INSTALLED`。
- **務必用 WSL 原生 FS（`~/`），不要 `/mnt/c`**：edk2 是海量小檔，`/mnt/c`（9p）寫入慢數倍。
- 工具鏈：
  ```bash
  sudo apt-get update
  sudo apt-get install -y build-essential uuid-dev iasl nasm \
    gcc-aarch64-linux-gnu python3 python3-pip python3-setuptools git
  ```
- **`python` 別名**：BaseTools 的 Tests 步驟呼叫 `python`（非 python3）。建別名：
  ```bash
  mkdir -p ~/bin && ln -sf "$(which python3)" ~/bin/python && export PATH="$HOME/bin:$PATH"
  ```

## 2) 取得 source（單線 + 防 auth hang）
```bash
cd ~
git clone https://github.com/worproject/rpi5-uefi.git
cd rpi5-uefi
export GIT_TERMINAL_PROMPT=0          # ★ 關鍵：遇要帳密立即失敗，不要 hang
# top-level 4 個 submodule（單線；不要 -j4 多線，慢網路會更慢/卡）
git submodule update --init --depth 1 edk2 edk2-platforms edk2-non-osi arm-trusted-firmware
# edk2-platforms 若只剩 gitlink 沒展開，強制 checkout：
git -C edk2-platforms checkout -f HEAD
```

### edk2 的 sub-submodule（build 解析 .dsc 需要這些都在）
```bash
cd ~/rpi5-uefi/edk2
git submodule update --init --depth 1 \
  CryptoPkg/Library/OpensslLib/openssl \
  CryptoPkg/Library/MbedTlsLib/mbedtls \
  MdeModulePkg/Library/BrotliCustomDecompressLib/brotli \
  BaseTools/Source/C/BrotliCompress/brotli \
  MdePkg/Library/MipiSysTLib/mipisyst \
  MdePkg/Library/BaseFdtLib/libfdt \
  ArmPkg/Library/ArmSoftFloatLib/berkeley-softfloat-3 \
  MdeModulePkg/Universal/RegularExpressionDxe/oniguruma \
  UnitTestFrameworkPkg/Library/CmockaLib/cmocka \
  UnitTestFrameworkPkg/Library/GoogleTestLib/googletest
```

### ★ subhook 的坑（最大時間殺手）
- worproject 的 `.gitmodules` 指向 **`https://github.com/Zeex/subhook.git`，此 repo 已 404 消失**。
- 匿名 clone 不存在的 repo → GitHub 回 401 → git **跳帳密提示**；在背景/非互動模式會**永遠 hang**（這就是先前「怎麼那麼久」的真相，不是網速也不是多線）。
- **解法：改用 tianocore 鏡像 `tianocore/edk2-subhook`**（其 HEAD 正好＝pin 的 `83d4e1e`）：
  ```bash
  cd ~/rpi5-uefi/edk2
  SH=UnitTestFrameworkPkg/Library/SubhookLib/subhook
  rm -rf "$SH" ".git/modules/$SH"
  GIT_TERMINAL_PROMPT=0 git clone --depth 1 https://github.com/tianocore/edk2-subhook.git "$SH"
  ```

> 註：`git fetch --depth 1 origin <SHA>` 只在 repo server 允許「fetch 任意 SHA」時才行（googletest 允許、有些不允許→改整包 clone 或用鏡像）。

## 3) Build
```bash
export PATH="$HOME/bin:$PATH"          # python 別名
export GIT_TERMINAL_PROMPT=0
cd ~/rpi5-uefi
bash build.sh --model 5                # 內部：build TF-A → EDK2(-a AARCH64 -t GCC -b RELEASE)
```
- ★ toolchain tag 是 **`GCC`**（新版 EDK2 統一了，**不是 `GCC5`**；用 GCC5 會報 `[GCC5] not defined`）。
- 成功後 `RPI_EFI.fd` 會在 `~/rpi5-uefi/RPI_EFI.fd`（build.sh 會 `cp` 到該目錄）。

## 4) 取出產出
```bash
cp ~/rpi5-uefi/RPI_EFI.fd /mnt/c/ai_project/AprWindowsDriver/uefi_build/
```
丟 SD 卡換掉原版 `RPI_EFI.fd`（見 `uefi_build/README.md`）。

## 踩坑速查表
| 症狀 | 原因 | 解 |
|------|------|----|
| `wsl` 報 `HCS_E_HYPERV_NOT_INSTALLED` | 虛擬化沒開 | BIOS 開 SVM/VT-x + `wsl --install --no-distribution` + 重開機 |
| `PeiUniCore.inf not found` | 抓成 tianocore edk2 master | 用 worproject/edk2 `sdmmc-dev`（rpi5-uefi 的 submodule）|
| `[GCC5] not defined` | 新版 EDK2 改 tag | 用 `-t GCC` |
| BaseTools `python: not found` | 只有 python3 | `ln -sf python3 ~/bin/python` + PATH |
| clone「怎麼那麼久」/卡住 | subhook repo 404 → 帳密提示 hang | `GIT_TERMINAL_PROMPT=0` + 用 `tianocore/edk2-subhook` 鏡像 |
| 下載很慢 | 多線 `-j4` 慢網路搶頻寬 | 單線（不加 -j） |
| `/mnt/c` 上 build 超慢 | 9p 檔案系統 | 改 WSL 原生 `~/` |
| `<某submodule>/include not found` | 該 edk2 sub-submodule 沒 init | 補 init（見 2 的清單）|
