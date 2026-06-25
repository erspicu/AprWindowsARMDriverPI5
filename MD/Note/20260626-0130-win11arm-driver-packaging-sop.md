# Pi5 Win11-ARM 封裝 SOP：ACPI 進 UEFI + 驅動 DISM 注入映像（2026-06-26）

> 用途：實機驗證「驅動無誤」後，把成果做成「**SD 卡插上 → 開機 → 自動列舉+載驅動**」的 appliance。
> 關鍵原則：**裝置描述(ACPI) 進韌體；驅動本體(.sys) 進 Windows 映像**——兩邊各司其職，驅動**不能**塞進 UEFI。

## 0) 觀念釐清（為何驅動不進 UEFI）
- **UEFI DXE 驅動** ≠ **Windows 驅動**：模型/ABI/執行環境完全不同。UEFI 驅動跑在開機前（GOP/SD/USB/PXE 等開機必需項，worproject 已有）；我們的 `*.sys`(KMDF/GpioClx/SpbCx…) 跑在 Windows 核心、由 PnP 載入。
- 韌體對 Windows 的正確整合點 = **ACPI 表**（`Rp1.asi`，已做）。它讓裝置「被 Windows 看見」，Windows 再去載對應 `.sys`。
- 決策見 `20260625-2230-pi5-uefi-acpi-integration.md`（主線=客製 UEFI ACPI）。

## 1) Appliance 兩半
| 半邊 | 內容 | 我們的產出 |
|------|------|-----------|
| **韌體（UEFI）** | ACPI(裝置描述) + 開機用 DXE(GOP/SD/USB) | `uefi_build/RPI_EFI.fd`（含 RP1 周邊 ACPI + 自訂 logo）|
| **Windows 映像** | 我們**簽章後的** `.sys`+`.inf` 預灌進 driver store | `windows_driver/<類別>/*.sys`+`*.inf` |

## 2) 簽章（注入前必做）
- **開發期**：目標機 `bcdedit /set testsigning on` + 驅動用測試憑證簽（`makecert`/`signtool`）即可載。
- **正式**：ARM64 驅動需正式簽章。選項：
  - 自簽 + 把憑證灌進映像的信任根（自用/內部）。
  - 送 **Microsoft 硬體開發者中心**做 **Attestation 簽章**（免完整 WHQL，較快）或完整 **WHQL/HLK**（要過 HLK 測試，正式發佈用）。
- 簽完才注入，否則 PnP 不載（除非 testsigning）。

## 3) DISM 注入驅動到離線 Windows 映像（WIM）
在一台 Windows（x64 或 ARM64）上對 Win11-ARM 的 `install.wim` 操作：
```powershell
# 掛載映像（index 視版本）
dism /Mount-Image /ImageFile:C:\img\install.wim /Index:1 /MountDir:C:\mnt
# 遞迴注入我們所有驅動（資料夾內含 .inf）
dism /Image:C:\mnt /Add-Driver /Driver:C:\ai_project\AprWindowsDriver\windows_driver /Recurse
# （可選）/ForceUnsigned 僅限開發；正式應為已簽章驅動
dism /Unmount-Image /MountDir:C:\mnt /Commit
```
- 結果：driver store 內預置我們的驅動 → 開機 PnP 對到 `ACPI\RPIF000n` 等就自動載入，使用者免手動 `pnputil`。
- **替代（開發快速路）**：實機開機後直接 `pnputil /add-driver xxx.inf /install`（不改映像，逐裝置驗證用）。
- **替代（量產友善）**：做 **provisioning package**（`.ppkg`）或 OEM `$WinPEDriver$` / `$OEM$` 機制。

## 4) 組裝 SD 卡
worproject SD（FAT32 開機分割）的檔基本不動，只換韌體：
1. **韌體**：用 `uefi_build/RPI_EFI.fd` 取代 SD 上的 `RPI_EFI.fd`（其餘 `config.txt`/Pi 韌體/TF-A/dtb 沿用；備份原檔）。
2. **OS**：Windows 分割用「已 DISM 注入驅動」的映像（WoR 工具部署，或先部署再線上注入）。
3. 開機 → 看到我們的 logo → 裝置自動列舉+載驅動。

## 5) 順序總結（實機驗證 OK 後）
```
驗證驅動(實機 pnputil 逐一過) → 簽章 → DISM 注入 install.wim → 部署到 SD 的 Windows 分割
                                                  ↑
                              SD 開機分割放 uefi_build/RPI_EFI.fd（含 ACPI）
```

## 待辦（到實機封裝階段再執行）
- 確認驅動簽章路線（自簽 vs Attestation vs WHQL）。
- 確認 WoR 部署 Win11-ARM 映像的實際流程（線上注入 vs 離線 WIM）。
- 驅動 INF 的 HardwareId 對齊 `ACPI\RPIF000n`（GPIO/I2C/SPI/UART… 已定義於 `uefi_fixed/.../Rp1.asi`）。
