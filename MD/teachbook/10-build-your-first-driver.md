# 第 10 章：建置工具鏈與你的第一個驅動（手把手 toolchain）

> 目標：把工具鏈實際走一遍——從環境、建置一個 `.sys`、跑 x64 模擬、編 ACPI、驗 INF。
> 讀完你就能自己接著移植下一個裝置。

## 10.1 環境（一次性）

| 元件 | 用途 | 備註 |
|------|------|------|
| **VS2022 + MSVC v143 ARM64 build tools** | 交叉編譯器 `cl`/`link` | 工作負載要勾「ARM64 build tools」 |
| **WDK 10.0.26100** | 驅動標頭 + lib + `asl.exe`/`infverif` | winget 版**不含** VS 的 MSBuild Driver 整合（無 `WDK.vsix`） |
| **x64 開發機**（本專案是 Win11 x64） | 跑 cl、跑 x64 模擬 | 交叉編譯出 ARM64 .sys |

> 因為沒有 MSBuild Driver 整合，本專案**不走 .vcxproj**，改用各驅動的 `build.ps1`（直接呼叫 cl/link）。
> 這反而更透明——你看得到每個 compile/link 旗標。

## 10.2 `build.ps1` 在做什麼（解剖）

一個典型 KMDF 驅動的 `build.ps1` 骨架：
```powershell
$Kit = "C:\Program Files (x86)\Windows Kits\10"
$Vc  = (Get-ChildItem "$VsRoot\VC\Tools\MSVC" | Sort Name -Desc)[0]
$Cl  = "$($Vc.FullName)\bin\Hostx64\arm64\cl.exe"     # x64 host → arm64 target
$Link= "$($Vc.FullName)\bin\Hostx64\arm64\link.exe"

# ── include 順序很重要：KMDF → km\crt → km → shared（勿加 VC user-mode include）──
$Inc = @("/I$Kit\Include\wdf\kmdf\1.33",
         "/I$Kit\Include\$Sdk\km\crt", "/I$Kit\Include\$Sdk\km",
         "/I$Kit\Include\$Sdk\shared")

# ── 編譯每個 .c ──
foreach ($s in Get-ChildItem *.c) {
    & $Cl /nologo /c /W4 /O2 /kernel /D_ARM64_ /DARM64 $Inc /Fo"build\ARM64\" $s.FullName
}

# ── 連結（KMDF：entry FxDriverEntry + wdf lib）──
& $Link /DRIVER /SUBSYSTEM:NATIVE,10.0 /MACHINE:ARM64 /NODEFAULTLIB /INTEGRITYCHECK `
        /ENTRY:FxDriverEntry `
        /LIBPATH:"$Kit\Lib\$Sdk\km\arm64" /LIBPATH:"$Kit\Lib\wdf\kmdf\arm64\1.33" `
        wdfdriverentry.lib wdfldr.lib libcntpr.lib BufferOverflowFastFailK.lib `
        ntoskrnl.lib hal.lib `
        build\ARM64\*.obj /OUT:build\ARM64\<驅動>.sys
```

**三個你一定會踩的點：**
1. **include 順序**：`wdf\kmdf\1.33` → `km\crt` → `km` → `shared`。順序錯 → 標頭衝突。
2. **不要加 VC user-mode include**：kernel 與 user-mode 標頭混用會爆。
3. **entry/lib 依框架不同**：KMDF=`FxDriverEntry`+wdf lib；非 KMDF=`GsDriverEntry`；
   GpioClx 加 `msgpioclxstub.lib`、SdPort 加 `sdport.lib`、NDIS 加 `ndis.lib`…（速查見 [第 03 章](03-windows-driver-frameworks.md)）。

## 10.3 建置一個驅動

```powershell
cd windows_sources\i2c\rp1-dw-i2c
.\build.ps1
# == Compiling (ARM64, KMDF) ==
# == Linking ==
# == DELIVERED to ...\windows_driver\i2c ==   ← 產出 rp1i2c.sys
```
產出的 `.sys` 是 **ARM64**，在 x64 機器上**不能執行**——它要搬到 Pi5/Win11-ARM 才載入。
（x64 端能做的「驗證」是下一步的模擬。）

## 10.4 跑 x64 模擬（不需硬體驗邏輯）

```powershell
# 用一般 x64 cl（不是交叉編譯），-D<X>_SIM 切到 mock 後端
cl /nologo /DDWI2C_SIM /I. sim\dwi2c_sim.c dw_i2c_hw.c /Fe:..\..\..\temp\dwi2c_sim.exe
..\..\..\temp\dwi2c_sim.exe
#   [PASS] CON register written
#   [PASS] SDA_HOLD RX-hold workaround set (bit16)
#   ...
# == 18 passed, 0 failed ==        ← 全綠 = 🔵 邏輯完整
```
（模擬原理見 [第 07 章](07-x64-simulation-pattern.md)。）

> 小技巧：多個 .c 一起編時，`/Fo` 要給**目錄**（結尾帶 `\`），不要給單一檔名，否則 cl 報 D8036。

## 10.5 編 ACPI

```powershell
$asl = "$Kit\Tools\$Sdk\x64\ACPIVerify\asl.exe"
& $asl windows_sources\pcie-rp1\acpi\rp1.asl
# Compliant with the ACPI 5.0 Specification
# asl(rp1.aml): Image Size=4529 ...     ← 產出 rp1.aml
```

## 10.6 驗 INF

```powershell
$iv = "$Kit\Tools\$Sdk\x64\infverif.exe"
& $iv /v windows_driver\i2c\rp1i2c.inf
# 只有 WARNING(1199)（DIRID 13 TargetOSVersion）= 可接受的 cosmetic
# （此環境 ERROR(1000) line 0 為共通假象，連已驗證 INF 也會出現，看實質 ERROR）
```

INF 最小骨架（KMDF）：
```ini
[Version]
Signature=$WINDOWS NT$
Class=System
ClassGuid={4D36E97D-E325-11CE-BFC1-08002BE10318}
PnpLockdown=1
[Standard.NTARM64]
%D%=rp1i2c_Device, ACPI\RPI50001        ; 綁到 ACPI 的 _HID
[rp1i2c_Device.NT.Wdf]
KmdfService=rp1i2c, rp1i2c_wdfsect
[rp1i2c_wdfsect]
KmdfLibraryVersion=1.33
```

## 10.7 接下來（到 Pi5 才做）

x64 端到此完成「邏輯 + 模擬 + ACPI/INF」。搬到 **Pi5 + Win11-ARM** 後：
1. `bcdedit /set testsigning on` + 自簽 `.sys`/`.cat`（[第 09 章](09-arm64-and-signing.md)）。
2. `pnputil /add-driver rp1i2c.inf /install`。
3. 載入 `rp1.aml`（平台 ACPI）。
4. KDNET 雙機接 WinDbg，校時序/中斷/DMA → 把 🔵 推到 ✅。

## 10.8 你的第一個移植（練習）

挑一個簡單的 bespoke KMDF（如 **RNG** 或 **PWM**）：
1. 讀 Linux 源碼（`iproc-rng200.c` / `pwm-rp1.c`）→ 抄暫存器邏輯到 `<dev>_hw.c`（[第 06 章](06-porting-methodology.md)）。
2. 寫 regio shim + sim，跑綠（[第 07 章](07-x64-simulation-pattern.md)）。
3. 寫 `driver.c`（KMDF：EvtDeviceAdd + EvtPrepareHardware map MMIO）+ ACPI device + INF。
4. `build.ps1` 出 .sys、`infverif` 驗、`asl.exe` 編。
5. 對照 [B9 範本章](B09-rp1-designware-i2c.md) 檢查有沒有漏 A/B 類規格（[第 08 章](08-hardware-truth-and-ab-gaps.md)）。

## 本章重點

- 環境：VS2022 ARM64 tools + WDK 10.0.26100；不走 .vcxproj，用 `build.ps1`（直接 cl/link）。
- 三大雷：**include 順序**、**不加 VC user-mode include**、**entry/lib 依框架**。
- 流程：`build.ps1`(出 .sys) → x64 sim(驗邏輯) → `asl.exe`(編 ACPI) → `infverif`(驗 INF)。
- ARM64 .sys 在 x64 不能跑；載入/簽章/偵錯到 Pi5 才做。

➡️ 回 [目錄](README.md)，開始第二部硬體章。
