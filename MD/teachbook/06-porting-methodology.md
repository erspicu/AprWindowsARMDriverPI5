# 第 06 章：移植方法論與開發流程

> 目標：把前面觀念串成一條**可操作的開發流程**——從「打開 Linux 驅動」到「Windows 上可載入、已驗證」。
> 這章是本書的「方法核心」，後面每個硬體章都照這套走。

## 6.1 核心招式：把驅動切成三層

```
┌─ OS glue（介接層，重寫）  driver.c / ddi.c：列舉、中斷、DMA、電源、接 Cx 框架
│        │ 呼叫
├─ HAL（硬體抽象層，沿用邏輯） <dev>_hw.c/.h：純暫存器狀態機，參數只有 void* Base
│        │ 透過
└─ regio shim（讀寫殼）       <dev>_regio.h：真機 READ/WRITE_REGISTER；模擬時換成 mock
```

**HAL 不含任何 OS 概念**（沒有 clock/DMA/中斷/鎖），只有「對暫存器做什麼」。
這帶來三個好處：
1. **可沿用 Linux 邏輯**（暫存器序列幾乎 1:1）。
2. **可在 x64 用模擬驗證**（把 regio shim 換成 mock，不需硬體就能跑狀態機——見 [第 07 章](07-x64-simulation-pattern.md)）。
3. **OS glue 只是翻譯**（呼叫 HAL + 接框架 callback）。

## 6.2 一個專案的標準長相

```
windows_sources/<類別>/<專案>/
├─ <dev>_hw.h / <dev>_hw.c   # HAL：暫存器定義 + 狀態機（void* Base）
├─ <dev>_regio.h             # 讀寫殼：真機 READ/WRITE_REGISTER / sim 用 mock
├─ driver.c (+ ddi.c)        # OS glue：KMDF/Cx 框架 callback
├─ common.h                  # 專案共用宣告
├─ sim/<dev>_sim.c           # x64 模擬 harness（mock 暫存器 + 斷言）
├─ sim/<dev>_simshim.h       # 「fake kernel」：sim 時的型別/巨集
└─ build.ps1                 # x64→ARM64 交叉編譯（直接呼叫 cl/link）
```

## 6.3 端到端移植流程（每個裝置都這樣做）

### Step 1：讀 Linux 源碼，找出「暫存器邏輯」
打開對應驅動（如 `drivers/i2c/busses/i2c-designware-*.c`），鎖定：
- 暫存器 offset 與 bit 定義（`#define DW_IC_*`）
- 初始化序列（`*_init` / `*_configure`）
- 傳輸引擎（`*_xfer` / `*_transfer_one`）
- 分頻/時序計算（常是 runtime 用 `clk_get_rate()` 算——記下公式）

> 把「**演算法**」抽出來，忽略 Linux 的 OS 部分（locking、IRQ 註冊、subsystem 註冊）。

### Step 2：寫 HAL（`<dev>_hw.c/.h`）
把暫存器序列翻成 `void* Base` 的純函式：
```c
// dw_i2c_hw.c —— 對照 Linux i2c_dw_configure_master() 重寫
VOID DwI2cHwConfigureMaster(PDW_I2C_HW Hw, DW_I2C_SPEED Speed) {
    DwI2cHwDisable(Hw);
    DwI2cWrite(Hw, DW_IC_CON, Hw->MasterCfg);
    DwI2cWrite(Hw, DW_IC_SS_SCL_HCNT, 0x0359);   // 分頻（用實測時脈算，見 Step 6）
    DwI2cWrite(Hw, DW_IC_SS_SCL_LCNT, 0x03E7);
    DwI2cWrite(Hw, DW_IC_INTR_MASK, 0);          // polled
}
```

### Step 3：寫 regio shim（`<dev>_regio.h`）
讓同一份 HAL，真機跑硬體、模擬跑 mock：
```c
#ifdef <X>_SIM
unsigned DwSimRd(unsigned off); void DwSimWr(unsigned off, unsigned v);
#define DwI2cRead(Hw,off)      DwSimRd(off)
#define DwI2cWrite(Hw,off,v)   DwSimWr(off, v)
#else
#define DwI2cRead(Hw,off)      READ_REGISTER_ULONG((volatile ULONG*)((Hw)->Base+(off)))
#define DwI2cWrite(Hw,off,v)   WRITE_REGISTER_ULONG((volatile ULONG*)((Hw)->Base+(off)), v)
#endif
```

### Step 4：寫 x64 模擬（`sim/<dev>_sim.c`）
mock 暫存器行為 + 斷言序列正確（不需硬體）：
```c
cl /D<X>_SIM /I.. sim\<dev>_sim.c ..\<dev>_hw.c /Fe:out.exe && .\out.exe
// 全 PASS → 邏輯（狀態機）正確 = 🔵 邏輯完整
```
（這步是本專案的品質關鍵，細節見 [第 07 章](07-x64-simulation-pattern.md)。）

### Step 5：寫 OS glue（`driver.c`）+ ACPI/INF
- 選對框架（[第 03 章](03-windows-driver-frameworks.md)），實作它的 callback，內部呼叫 HAL。
- `EvtPrepareHardware` map MMIO → 設成 HAL 的 `Base`。
- 在 `rp1.asl` 加裝置（`_HID`），寫對應 INF 綁 `ACPI\<_HID>`（[第 04 章](04-device-tree-to-acpi.md)）。

### Step 6：用實機校正「猜的值」
有些值 Linux 源碼只有公式、實際數字 runtime 才知道（時脈/絕對位址/IRQ）——
用 Pi5 SSH 量真矽晶片校正（**A 類缺漏**）；同時對照源碼補回移植時漏抄的暫存器設定（**B 類缺漏**）。
（見 [第 08 章](08-hardware-truth-and-ab-gaps.md)。）

### Step 7：建置 + 驗證
```powershell
.\build.ps1                              # 交叉編譯出 ARM64 .sys
infverif.exe /v ..\..\windows_driver\<類別>\<驅動>.inf   # 驗 INF
asl.exe windows_sources\pcie-rp1\acpi\rp1.asl            # 編 ACPI
```

## 6.4 狀態階梯：你的進度長什麼樣

| 狀態 | 意思 | 達成條件 |
|------|------|----------|
| 🟢 骨架 | 可建置、HAL 邏輯就位 | build 過 |
| 🔵 邏輯完整 | 狀態機全移植 + **x64 模擬驗證過** | sim 全 PASS |
| ✅ 完整 | 實機驗證（或純軟體交付物如 ACPI/INF） | Pi5 + Win11-ARM 載入驗證 |

> x64 端能把硬體驅動推到 **🔵**（邏輯+模擬）；**🔵→✅** 要 Pi5 實機（載入、中斷路由、DMA、KDNET 偵錯）。
> 這就是本專案的「雙機交接模型」。

## 6.5 Toolchain 速覽（細節見 Skill）

| 工作 | 工具 | 一行指令 |
|------|------|----------|
| 交叉編譯 .sys | `build.ps1`（cl/link） | `.\build.ps1` |
| x64 模擬 | `cl` | `cl /D<X>_SIM /I.. sim\x_sim.c ..\x_hw.c /Fe:o.exe` |
| 編 ACPI | `asl.exe` | `asl.exe rp1.asl` |
| 驗 INF | `infverif.exe` | `infverif /v x.inf` |

> 完整環境/include 順序/各框架 link lib → [`../Skill/build-toolchain.md`](../Skill/build-toolchain.md)；
> 手把手第一個驅動 → [第 10 章](10-build-your-first-driver.md)。

## 本章重點

- 三層切法：**OS glue（重寫）→ HAL（沿用邏輯）→ regio shim（讀寫殼）**。
- 流程：讀源碼 → 寫 HAL → regio shim → x64 sim → OS glue + ACPI/INF → 實機校正 → build/驗證。
- HAL 乾淨 → 可模擬、可沿用、glue 只是翻譯。
- 進度看狀態階梯：🟢→🔵（x64 模擬過）→✅（實機）。

➡️ 下一章：[x64 模擬驗證：不用硬體就能驗狀態機](07-x64-simulation-pattern.md)
