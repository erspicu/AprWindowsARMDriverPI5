# 第 07 章：x64 模擬驗證——不用硬體就能驗狀態機

> 目標：學會本專案的招牌品質手段——把 HAL 的暫存器邏輯，在 x64 開發機用「假暫存器 + 斷言」跑過一遍，
> **完全不需要 Pi5**，就能確認狀態機正確（達到 🔵 邏輯完整）。

## 7.1 為什麼可以模擬？

因為 HAL 是**純函式**：輸入只有 `void* Base`，做的事只有「對某個 offset 讀寫」。
只要把「讀寫」這個動作攔截下來，導到一塊**記憶體陣列 + 一點行為模型**，HAL 根本分不出自己跑在真硬體還是假的。

```
真機：  HAL ──讀寫──→ READ/WRITE_REGISTER ──→ 真 MMIO
模擬：  HAL ──讀寫──→ XxxSimRd/XxxSimWr   ──→ g_regs[] 陣列 + mock 行為 + 斷言
                      ↑ 由 regio shim 的 #ifdef <X>_SIM 切換
```

## 7.2 四個零件

```
sim/<dev>_sim.c        # 主程式：建立假暫存器、呼叫 HAL、斷言結果
sim/<dev>_simshim.h    # 「fake kernel」：定義 sim 用的型別/巨集（UINT32、READ_REGISTER… 的替身）
<dev>_regio.h          # 讀寫殼：#ifdef <X>_SIM 時把讀寫導到 XxxSimRd/Wr
<dev>_hw.c             # 被測對象（與真機同一份！）
```

關鍵：**HAL 原始碼一字不改**，只靠編譯期 `-D<X>_SIM` 切換 regio shim。

## 7.3 最小範例

**regio shim（同一份 HAL，兩種後端）：**
```c
#ifdef DWI2C_SIM
unsigned DwSimRd(unsigned off);  void DwSimWr(unsigned off, unsigned v);
#define DwI2cRead(Hw,off)     DwSimRd(off)
#define DwI2cWrite(Hw,off,v)  DwSimWr(off,(v))
#else
#define DwI2cRead(Hw,off)     READ_REGISTER_ULONG((volatile ULONG*)((Hw)->Base+(off)))
#define DwI2cWrite(Hw,off,v)  WRITE_REGISTER_ULONG((volatile ULONG*)((Hw)->Base+(off)),(v))
#endif
```

**sim 主程式（mock + 斷言）：**
```c
static unsigned char g_regs[0x100];
static unsigned load(unsigned off){ /* 從 g_regs 組 32-bit */ }
static void store(unsigned off, unsigned v){ /* 寫回 g_regs */ }

unsigned DwSimRd(unsigned off) {
    switch (off) {
    case DW_IC_COMP_TYPE: return DW_IC_COMP_TYPE_VALUE;        // 假裝身分驗證過
    case DW_IC_STATUS:    return DW_IC_STATUS_TFNF|DW_IC_STATUS_TFE;  // FIFO 永遠可寫
    default:              return load(off);
    }
}
void DwSimWr(unsigned off, unsigned v){ store(off, v); /* 必要時模型化副作用 */ }

static void check(const char* what, int cond){ cond ? pass(what) : fail(what); }

int main(void) {
    DW_I2C_HW hw = {0};
    DwI2cHwProbe(&hw);
    DwI2cHwConfigureMaster(&hw, DwI2cFast);
    check("CON 寫入正確", load(DW_IC_CON) == hw.MasterCfg);
    check("SDA_HOLD RX-hold 設了", (load(DW_IC_SDA_HOLD) & 0xFF0000) == (1<<16));
    // ... 跑 write/read 序列，斷言 DATA_CMD 內容、STOP bit、收到的 byte ...
    printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
```

## 7.4 怎麼建置 + 跑

```powershell
cl /DDWI2C_SIM /I.. sim\dwi2c_sim.c ..\dw_i2c_hw.c /Fe:temp\dwi2c_sim.exe
.\temp\dwi2c_sim.exe
# == 18 passed, 0 failed ==   ← 全 PASS = 狀態機正確 = 🔵 邏輯完整
```
- 用一般 **x64 cl**（不是交叉編譯），因為 sim 是普通 user-mode 程式。
- exe 丟 `temp/`（暫存）。

## 7.5 mock 要模型化「硬體會回什麼」

模擬不是把暫存器當啞陣列，而是要model**硬體的回應**，狀態機才走得下去。常見幾種：
- **身分暫存器**：直接回 magic（如 COMP_TYPE）讓 probe 過。
- **status/ready bit**：回「永遠就緒」或「寫了 START 後就 READY」。
- **FIFO**：寫 DATA_CMD 記到一個 log 陣列；讀則從預設來源吐 byte。
- **self-clearing bit**：寫 RESET 後下次讀就清掉。
- **W1C（write-1-clear）**：寫 1 的位元清掉。

> 例：ADC 模擬「寫 START_ONCE → 下次讀 CS 就帶 READY、RESULT 給一個值」，
> 才能驗 `Convert()` 的「啟動→等就緒→讀結果」整條路。

## 7.6 模擬能驗到什麼、不能驗到什麼

| 能驗 ✅ | 不能驗 ❌（要實機） |
|---------|---------------------|
| 暫存器寫入順序與內容 | 真實時序 / 訊號完整性 |
| bit field 組對沒 | 中斷實際路由（MSI-X→GIC） |
| 狀態機分支（讀/寫/STOP/多筆） | DMA 與 cache coherency |
| 分頻/常數算對沒（對照實測值） | 框架 callback 在真 OS 的行為 |

→ 模擬把驅動推到 **🔵 邏輯完整**；**🔵→✅** 仍需 Pi5 實機（[第 08 章](08-hardware-truth-and-ab-gaps.md)）。

## 7.7 為什麼這招很值

- **快**：改完 HAL 幾秒內知道對不對，不用燒 image、不用實機。
- **回歸**：每次改規格（如修分頻）跑一次 sim，確認沒弄壞別的。
- **當規格活文件**：斷言本身就是「這個暫存器序列應該長這樣」的可執行說明。

本專案 14+ 個引擎都有 sim，數百條斷言全綠——這是「x64 端能保證的最高品質」。

## 本章重點

- HAL 是純函式 → 把「讀寫」用 `#ifdef <X>_SIM` 導到 mock，就能在 x64 跑。
- 四零件：`sim.c`（mock+斷言）+ `simshim.h`（fake kernel）+ `regio.h`（切換）+ `hw.c`（被測，與真機同份）。
- mock 要**模型化硬體回應**（ready/FIFO/self-clear/W1C）。
- 全 PASS = 🔵 邏輯完整；時序/中斷/DMA 仍需實機。

➡️ 下一章：[用實機校正：Pi5 SSH 與「規格缺漏 A/B 兩類」](08-hardware-truth-and-ab-gaps.md)
