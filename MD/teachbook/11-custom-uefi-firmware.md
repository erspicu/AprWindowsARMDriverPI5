# 11. 客製 UEFI 韌體：為什麼、是什麼、怎麼做

> 前面第 4 章說「硬體描述要從 Device Tree 變成 ACPI」，但留了一個問題沒答：**在 Pi5 上，那些 ACPI 表到底「住在哪」、由誰提供給 Windows？**
> 答案是 **UEFI 韌體**。這章補上這塊——它既是讓 Windows 能在 Pi5 開機的**前提**，也是我們把硬體描述塞給 Windows 的**唯一入口**。
>
> 預備：讀過第 4 章（DT→ACPI）、第 5 章（PCIe/RP1 列舉）。

---

## 11.1 為什麼要「客製」UEFI？（四個理由，由硬到軟）

很多人第一反應是：「韌體不是廠商給好的嗎？幹嘛自己改？」Pi5 的情況很特別：

1. **Pi5 根本沒有官方的 Windows 韌體。**
   x86 PC 出廠就有支援 Windows 的 UEFI/BIOS；Pi5 的官方韌體只認得 Linux 開機鏈。要在 Pi5 上跑 Windows on ARM，**一定**得用社群（worproject）做的 EDK2 UEFI 放 SD 卡開機。**既然「刷客製韌體」本來就是必經步驟，那把我們要的東西也順手塞進去，等於零額外成本。** 這是整個專案選「做法 A（客製 UEFI ACPI）」的核心理由。

2. **上游 UEFI 只描述了一部分硬體。**
   worproject 的 UEFI 在 ACPI 裡只描述了 RP1 底下的 **USB**（所以 stock Win11-on-Pi5 鍵鼠能動、其他不動）。**RP1 的 GPIO/I2C/SPI/UART… 在 Windows 裡「看不到」**——因為沒有人在 ACPI 告訴 Windows 它們存在。第 4 章說「Windows 靠 ACPI 列舉非 PnP 裝置」，這裡就是現場：要讓 Windows 看到我們的裝置、載我們的驅動，**就得在 UEFI 的 ACPI 加上 Device 節點**。

3. **記憶體/平台層的硬體相容問題只能在韌體解。**
   例如 16GB 版 Pi5 卡開機（見 11.6），或記憶體 map、TF-A 的 DRAM 設定——這些是「Windows kernel 接手前」的事，OS 層碰不到，只能改韌體。

4. **概念連結（重要）**：**ACPI 表「住在 UEFI 韌體裡」。** 你想改「Windows 開機時看到的硬體長相」，唯一辦法就是改 UEFI 重新 build。這跟 x86 PC「進 BIOS 設定」是同一件事的底層版。

> 一句話：**在 Pi5 上，UEFI = 「給 Windows 的硬體說明書」+「開機載入器」。改說明書 = 改 UEFI。**

## 11.2 UEFI 是什麼？（技術介紹）

### 開機鏈：從通電到 Windows 桌面
```
EEPROM(Pi 韌體) → VideoCore(讀 config.txt) → TF-A(Trusted Firmware-A, 安全韌體)
  → EDK2 UEFI(RPI_EFI.fd) → Windows Boot Manager(bootmgfw.efi)
  → Windows OS Loader(winload.efi) → Windows Kernel(ntoskrnl.exe)
```
- **EDK2**＝開源的 UEFI 實作（TianoCore）。worproject 把它移植到 Pi5，產出一個 `RPI_EFI.fd`（韌體映像）放 SD 卡。
- **TF-A**＝ARM 的安全世界韌體（設定 CPU/記憶體/安全狀態），在 UEFI 之前跑。

### UEFI 交給作業系統的四樣東西
1. **記憶體 map**（哪段實體位址是 RAM、哪段是 MMIO）——Windows 從這裡知道有多少記憶體。
2. **ACPI 表**（DSDT/SSDT/MADT/MCFG…）——硬體與資源描述。**這是我們要改的。**
3. **開機服務 + DXE 驅動**——開機階段要用的：GOP（畫面/開機 logo）、SD/NVMe（載 OS）、USB（鍵盤）、網路（PXE）。
4. **交棒**：呼叫 `ExitBootServices` 後把控制權交給 Windows Boot Manager。

### UEFI DXE 驅動 ≠ Windows 驅動（呼應「驅動不進 UEFI」）
- **UEFI DXE 驅動**：跑在**開機前**的韌體環境，C 寫的 PE module，只做開機要用的事（顯示/儲存/USB）。
- **我們的 Windows 驅動（.sys）**：跑在 **Windows 核心**裡、開機後由 PnP 載入。
- **兩者模型/ABI 完全不同、不能互換。** 所以韌體放**裝置描述（ACPI）**，驅動本體用 DISM 注入 Windows 映像（見第 9 章封裝）。

### ACPI 在 UEFI 裡長怎樣
ACPI 表是 **ASL（ACPI Source Language）** 寫的 `.asl/.asi` 檔，build 時用 **iasl** 編成 `.aml` 二進位、包進 `.fd`。我們加一個裝置就是在 `.asl` 裡加一段：
```asl
Device (GPI0) {                       // RP1 GPIO controller
  Name (_HID, "RPIF0001")             // 給 Windows 認的硬體 ID；驅動 INF 來 match
  Name (_UID, 0x0)
  Method (_CRS, 0, Serialized) {      // 回報資源：MMIO 窗口 + 中斷
    Name (RBUF, ResourceTemplate () {
      QWordMemory (...) ; Interrupt (...) { 282 }
    })
    ... PBAR + RP1_IO_BANK0_BASE ...   // 位址 = 韌體 patch 的 BAR + 周邊 offset
    Return (RBUF)
  }
}
```
→ 開機後 Windows 的 `acpi.sys` 解析這段，建立一個 `ACPI\RPIF0001` 的裝置節點，PnP 就會去找 INF 寫著 `ACPI\RPIF0001` 的驅動載入。**這就是第 4 章「DT→ACPI」在 Pi5 的落地。**

### FV / FD 結構（為何「加東西不怕爆」）
`RPI_EFI.fd`（韌體檔，固定 2MB）內含多個 **FV（Firmware Volume）**。我們的 ACPI/DXE 在 `FvMain`，它**按內容自動長大**，再被 LZMA 壓進 `FVMAIN_COMPACT`（flash 上 1.75MB 區）。實測：加 21 個 ACPI 節點 raw 才 +4KB，壓縮後僅 +712B → 壓縮容器還剩 ~526KB，**塞幾百個節點都沒問題**。

## 11.3 開發方式（怎麼客製 + build）

### 來源：別搞錯 repo
- build harness = **`worproject/rpi5-uefi`**（含 `build.sh` + 4 個 submodule，都是 **worproject fork 的特定分支**，**不是** tianocore master）：edk2 `sdmmc-dev`、edk2-platforms `rpi5-dev`、TF-A `rpi5`、edk2-non-osi。
- 環境：**WSL2 Ubuntu**（需開硬體虛擬化）；toolchain tag **`GCC`**（新版 EDK2 不再是 GCC5）；要 `iasl`、`aarch64-linux-gnu-gcc`。

### overlay 模型（不直接改上游）
本專案不去改上游樹，而是把我們的改動放 `uefi_fixed/`，**按組件鏡像上游路徑**，build 前 overlay 上去：
```
uefi_fixed/edk2-platforms/.../Rp1.asi        → 蓋到 ~/rpi5-uefi/edk2-platforms/.../Rp1.asi
uefi_fixed/edk2-platforms/.../Dsdt.asl       → V3D GPU 節點
uefi_fixed/edk2-platforms/.../RaspberryPiMem.c → 記憶體旋鈕
uefi_fixed/edk2-non-osi/.../Logo.bmp         → 開機 logo
```
一支 `uefi_fixed/build-uefi.sh` 自動 overlay → 跑 `build.sh` → 產出 `uefi_build/RPI_EFI.fd`。**好處：我們的改動清楚可見、可版控，上游更新時 re-merge 範圍小。**

### 三種改法
1. **加 ACPI 裝置**：在 `Rp1.asi`（RP1 周邊）或 `Dsdt.asl`（SoC 裝置如 V3D）加 `Device` 節點。
2. **改記憶體**：`RaspberryPiMem.c`（11.6）。
3. **換 logo**：替換 8-bit BMP（同尺寸即可，見 11.5 補充）。

### 驗證
- `iasl` 編譯 ASL（語法錯立刻報，例如 11.5 的 `_HID` hex 規則）。
- build log 看 `FvMain`/`FVMAIN_COMPACT` 容量。
- 完整踩坑配方見 `MD/Skill/pi5-uefi-build.md`。

## 11.4 把 ACPI 與驅動接起來（端到端）
```
UEFI Dsdt/Rp1.asi 的 Device(_HID="RPIF0001")
        ↓ 開機 acpi.sys 解析
裝置管理員出現 ACPI\RPIF0001
        ↓ PnP 配對
我們的 rp1gpio.inf (match ACPI\RPIF0001) → 載入 rp1gpio.sys
```
**這條鏈閉合，就是「客製 UEFI」帶來的價值**：本來 Windows 完全看不到的 RP1 周邊，現在能列舉 + 載驅動。

---

## 11.5 補充：適合教學、但容易被忽略的點

### (a) ACPI `_HID` 命名規則（一個真實 build error 教會你的事）
ACPI `_HID` 字串的**後 4 碼必須是 hex digit（0-9A-F）**。我們一開始用 `"RPI0GPIO"`，iasl 直接報 **`error 6035: _HID suffix must be all hex digits`**（後綴 `GPIO` 含非 hex 的 G/I/O）。改成 vendor `RPIF` + hex 流水號 `RPIF0001` 才過。**教訓：ACPI 命名有硬規則，且「同類控制器共用一個 _HID、用 _UID 區分實例」才是標準做法。**

### (b) 開發期捷徑：不必每次重 build/重刷 SD 卡
改 SoC 級 ACPI 時，可用 WDK 的 `asl.exe /loadtable` **在 Windows 開機後動態注入 SSDT**：寫好一段 `.asl` → `asl.exe` 編 → `asl.exe /loadtable ssdt.aml` → 重開機，裝置管理員就出現新裝置。**驗證階段超省時間**，定案後才併回 UEFI 重 build。

### (c) 「韌體側 vs OS 側」的分工 = appliance 心智模型
最終成品 = **SD 卡上的客製 UEFI（含 ACPI）** + **預先 DISM 注入驅動的 Windows 映像**。記住：**裝置「描述」進韌體；驅動「本體」進 OS。** 這個分工是 WoA 移植的通則，不只 Pi5。

### (d) 開機卡住怎麼診斷（通用 WoA 技能）
看得到 UEFI logo 但無 Windows spinner = **UEFI 交棒成功、但 winload/kernel 早期掛了**（常是 ACPI/記憶體）。診斷：
- 接 **UART 序列埠**看 log（最有效）；
- 或把 SD 卡插回電腦，改 BCD：`bcdedit /store <SD>:\EFI\Microsoft\Boot\BCD /set {default} sos yes` → 重開機會列出載入到哪個 `.sys` 卡住。

### (e) 「先讀碼再下結論」：total_mem 為何沒用
16GB 卡開機時，直覺是改 `config.txt` 的 `total_mem=8192` 限制記憶體——**但沒用**。讀 UEFI 源碼才發現：它用**板子 revision code** 算記憶體大小（`BoardRevisionGetMemorySize`），**根本不看 `total_mem`（那是 VideoCore mailbox 路徑）**。**教訓：症狀對不上時，去讀實際程式碼，別憑直覺猜。**（這也是本專案對 Gemini 答案的態度：給方向、細節對真 header/源碼查證。）

### (f) 開機 logo 從哪來？UEFI GOP + ACPI BGRT
你看到的 Raspberry Pi logo 是 **UEFI 的 `LogoDxe`** 畫到 GOP framebuffer，並登記成 ACPI **BGRT** 表，讓 Windows 開機初期延續同一張圖。換 logo＝換那個 8-bit BMP（**維持同尺寸/色深最保險**，EDK2 對 BMP 格式有要求；只改像素不動 header 可避免相容問題）。

### (g) 軟體保存意識：凍結可編譯備份
我們依賴的是 **個人 fork + 特定 commit**。實際踩到：subhook 的上游 `Zeex/subhook` **整個 repo 404 消失了**（改用 tianocore 鏡像才救回）。**教訓：依賴會蒸發。** 所以本專案把「能編譯的 source」凍結成備份（`uefi_sources_backup/`，端到端驗證過），確保來源全消失也能重建。這是做韌體/長壽專案該有的習慣。

---

## 11.6 案例研究：16GB Pi5 開機卡住（綜合演練）
把上面學的串起來：
- **症狀**：16GB 板看到 logo、無 spinner、進不去 Windows。
- **第一直覺（錯）**：`total_mem=8192` → 沒用（11.5e：UEFI 用 revision code，不看 total_mem）。
- **讀碼**：`RaspberryPiMem.c` 用 revision `e04171` 解出 16GB（bits[20:22]=6→256MB<<6=16GB，**正確**）；`>4GB` 的記憶體 map 碼**看起來也對**。
- **推論**：崩潰不在 UEFI 記憶體 map，而在**下游**（ACPI 記憶體描述 / TF-A DRAM / Windows kernel 對這套 16GB map 的早期處理）。
- **務實解 + 診斷**：在 `RaspberryPiMem.c` 加旋鈕 `RPI_RAM_CAP_GB`，把回報 RAM 壓到 8GB →（a）若能進＝確認是 16GB 記憶體問題、（b）若還卡＝另有原因。**這個「壓一半當解又當診斷」的手法很實用。**
- **完整解（待）**：接序列埠定位 kernel 死在哪一步，再對症修 ACPI/TF-A。

> 這個案例同時用到：讀源碼、ACPI/記憶體分層、UEFI overlay 改碼、序列埠診斷——是這章所有觀念的綜合演練。

---

## 小結
- 在 Pi5，**UEFI = 給 Windows 的硬體說明書 + 開機載入器**；沒有官方 Windows 韌體，所以客製是必經、且順手把 ACPI 塞進去零成本。
- 要讓 Windows 看到/載入我們的驅動，**就得在 UEFI 的 ACPI 加 Device 節點**（`_HID` 對齊驅動 INF）。
- 開發走 **overlay 模型**（`uefi_fixed/` → 上游樹 → WSL build → `RPI_EFI.fd`）。
- 韌體放**描述**、OS 放**驅動**；診斷開機用序列埠/BCD；依賴會消失要凍結備份。

> 對照實作：`uefi_fixed/`、`uefi_build/RPI_EFI.fd`、配方 `MD/Skill/pi5-uefi-build.md`、決策/封裝 `MD/Note/20260625-2230` 與 `20260626-0130`、總說明 `MD/HANDOFF.md` §7。
