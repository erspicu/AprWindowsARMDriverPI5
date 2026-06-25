# `uefi_sources_backup/` — 可編譯的 Pi5 UEFI source 凍結備份

> **用途**：保存「**經實測能 build 出 Pi5 Win11-ARM UEFI（`RPI_EFI.fd`）**」的精簡 source 快照，
> 以防哪天 worproject 的個人 fork / 上游鏡像消失（已經發生過：`Zeex/subhook` 整個 404 不見了）。
> 換機 / 來源失蹤時，靠這份就能重建並 build。建立：2026-06-26（已驗證 build 成功，2,031,616 bytes）。

## 為什麼需要這個
Pi5 UEFI 不是用 tianocore 官方 source，而是 **worproject 的 fork + 特定分支**（見下）。這些是**個人專案分支，隨時可能被刪或改名**——`Zeex/subhook` 就是活生生的例子（已 404）。一旦消失，沒有這份備份就 build 不出來了。

## 內容
| 檔案 | 說明 |
|------|------|
| `rpi5-uefi-src-min.tar.gz` | **精簡 source 樹**（已剝掉 .git 歷史、Build 產物、別家板子/CPU、可重抓的外部子模組）。解開即用。|
| `SUBMODULES.txt` | 被剝掉的「可重抓上游子模組」清單（path + url + **pin commit**）。 |
| `fetch-submodules.sh` | 依上表把那些子模組還原回 `edk2/<path>`（subhook 自動改用 tianocore 鏡像）。|
| `COMMITS.txt` | 各 worproject fork 的確切 commit（pin 用）。|

## 凍結的版本（worproject forks，脆弱、本備份的重點）
| 組件 | repo / branch | commit |
|------|---------------|--------|
| edk2 | worproject/edk2 `sdmmc-dev` | `eca8aae` |
| edk2-platforms | worproject/edk2-platforms `rpi5-dev` | `8e1779b`（僅留 RaspberryPi/Broadcom 子樹）|
| edk2-non-osi | tianocore/edk2-non-osi | `1f4d784`（僅留 Platform/RaspberryPi）|
| arm-trusted-firmware | worproject/arm-trusted-firmware `rpi5` | `682607f` |
| rpi5-uefi (wrapper) | worproject/rpi5-uefi | `a6135b0` |

## 怎麼從備份重建並 build（WSL Ubuntu）
```bash
# 1) 解開精簡 source
mkdir -p ~/rpi5-build && tar -xzf rpi5-uefi-src-min.tar.gz -C ~/rpi5-build
cd ~/rpi5-build

# 2) 還原可重抓的子模組（openssl 等；subhook 走鏡像）
bash /path/to/uefi_sources_backup/fetch-submodules.sh ~/rpi5-build/edk2

# 3) 工具鏈 + build（詳見 MD/Skill/pi5-uefi-build.md）
sudo apt-get install -y build-essential uuid-dev iasl nasm gcc-aarch64-linux-gnu python3 python3-setuptools git
mkdir -p ~/bin && ln -sf "$(command -v python3)" ~/bin/python && export PATH="$HOME/bin:$PATH"
export GIT_TERMINAL_PROMPT=0
bash build.sh --model 5
# 產出 ~/rpi5-build/RPI_EFI.fd
```

## 設計取捨
- **存**：worproject 三個 fork 的內容（會消失）＋ subhook 鏡像這個關鍵知識。
- **不存、只記 manifest**：openssl(198MB)/mbedtls/googletest 等**穩定上游**（github.com/openssl 等不會消失，pin commit 隨時重抓）→ 大幅縮小備份。
- 完整 build 步驟與所有踩坑：`MD/Skill/pi5-uefi-build.md`。我們的 ACPI 修改：`uefi_fixed/`。
