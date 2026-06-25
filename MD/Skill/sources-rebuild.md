# Skill：重建 `sources/` 參考源碼目錄（Pi Linux 核心 + Infineon WHD）

> `sources/` 是**移植參考源碼**（樹莓派 Linux 核心 + Infineon Wi-Fi Host Driver），**刻意不納版控**
> （`.gitignore` 第 4 行 `/sources/`，且各自帶獨立 `.git`，約 **1.5 GB**）。本檔讓**另一台機器（如 Win11 ARM 開發機）的
> Claude Code 重建出與本機一致的參考目錄**，以便查暫存器 offset / 初始化序列 / 演算法。
> ⚠️ 注意：`sources/` 內的檔案**不會**出現在 git clone 裡——所以本指南放在受控的 `MD/Skill/`，clone 後即可讀。

## 用途與原則
- 只當**唯讀參考**：我們「重寫」而非編譯它（見 `CLAUDE.md` 移植原則）。不需要能 build 核心，只要原始碼可查。
- 用 **blobless 部分 clone（`--filter=blob:none`）+ sparse-checkout**，把 1.5GB 控制在最小、只取會用到的子樹。

---

## 1) 樹莓派 Linux 核心（移植主要參考）
- repo：`https://github.com/raspberrypi/linux.git`
- 分支：`rpi-6.18.y`　｜　版本：Linux **6.18**
- **釘選 commit**：`954341c412dd48b7c7f8125d81212ec4c0e42ed3`（確保與本機逐字一致）

```bash
cd C:/ai_project/AprWindowsDriver        # 專案根
# blobless 部分 clone（blob 按需下載），先不 checkout
git clone --filter=blob:none --no-checkout --branch rpi-6.18.y \
    https://github.com/raspberrypi/linux.git sources
cd sources
# sparse-checkout（cone 模式）：只取會用到的子樹
git sparse-checkout init --cone
git sparse-checkout set \
    Documentation/devicetree/bindings \
    arch/arm/boot/dts/overlays \
    arch/arm64/boot/dts/broadcom \
    drivers \
    include \
    sound/core \
    sound/soc
# 釘選到本機同一個 commit
git checkout 954341c412dd48b7c7f8125d81212ec4c0e42ed3
```

> 想要完整核心樹（較大、較慢）就略過 sparse 步驟，直接 `git clone --branch rpi-6.18.y ... sources && cd sources && git checkout <commit>`。

## 2) Infineon Wi-Fi Host Driver（WHD，CYW43455 WiFi 用）
- repo：`https://github.com/Infineon/wifi-host-driver`
- 分支：`master`（tag 線 `latest-v5.X`）
- **釘選 commit**：`bd99da7b005c97500d7c697f544b36fabdaa3f5e`

```bash
cd C:/ai_project/AprWindowsDriver/sources
git clone https://github.com/Infineon/wifi-host-driver.git wifi-host-driver
cd wifi-host-driver
git checkout bd99da7b005c97500d7c697f544b36fabdaa3f5e
```
WHD 子目錄重點：`WHD/`（核心庫）、`External/`、`deps/`、`docs/`。我們的 port layer 參照其 `whd_*` API。

---

## 3) 驗證（重建後跑這個，全 ✓ 才算對）
```bash
cd C:/ai_project/AprWindowsDriver
for d in arch/arm64/boot/dts/broadcom drivers/gpu/drm/vc4 drivers/gpu/drm/v3d \
         drivers/pinctrl drivers/media/platform/raspberrypi drivers/nvmem \
         drivers/thermal/broadcom include/soc/bcm2835 sound/soc \
         wifi-host-driver/WHD; do
  [ -d "sources/$d" ] && echo "  ✓ $d" || echo "  ✗ $d (缺)"
done
git -C sources rev-parse HEAD                    # 應為 954341c4...
git -C sources/wifi-host-driver rev-parse HEAD   # 應為 bd99da7b...
```

## 重點檔案（移植時常查）
| 子裝置 | 參考路徑 |
|--------|---------|
| RP1 周邊位址/IRQ/pinmux | `arch/arm64/boot/dts/broadcom/rp1.dtsi`、`rp1-common.dtsi` |
| BCM2712 SoC DT | `arch/arm64/boot/dts/broadcom/bcm2712*.dtsi` |
| GPIO/pinctrl | `drivers/pinctrl/pinctrl-rp1.c` |
| 顯示 vc4/HVS/HDMI | `drivers/gpu/drm/vc4/`（`vc4_regs.h`/`vc4_hvs.c`/`vc4_hdmi*.c`） |
| GPU V3D | `drivers/gpu/drm/v3d/`（`v3d_regs.h`） |
| 相機/HEVC/ISP | `drivers/media/platform/raspberrypi/`（`rp1-cfe`/`hevc-dec`/`pisp-be`） |
| 溫度/AVS | `drivers/thermal/broadcom/`、`bcm2712*` thermal coeff 在 DT |
| OTP | `drivers/nvmem/raspberrypi-otp.c` |
| 韌體 mailbox | `include/soc/bcm2835/raspberrypi-firmware.h` |
| UART PL011 | `include/linux/amba/serial.h` |

## 備註
- 兩個 repo 各自帶 `.git`，是獨立 repo；本專案 git **不追蹤** `sources/`（故 clone 不含它，要照本檔重建）。
- 若硬碟吃緊：blobless+sparse 後仍想再瘦，可再縮小 `git sparse-checkout set` 的清單（只留上表用到的 driver 子目錄）。
