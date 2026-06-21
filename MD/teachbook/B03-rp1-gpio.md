# B3：RP1 GPIO / pinctrl（GpioClx）

> 第一個 **GpioClx** 範例——和 SpbCx 很不一樣：GpioClx 要實作約 16 個 callback，且 GPIO 兼具
> 「讀寫腳位」與「中斷控制器」兩種身分。RP1 GPIO 還示範了 **atomic 別名暫存器** 這個常見硬體技巧。

| | |
|---|---|
| **裝置** | RP1 GPIO（40-pin 排針上的 GPIO 走這），3 個 bank（pin 0-27 / 28-33 / 34-53） |
| **Linux 源碼** | `drivers/pinctrl/pinctrl-rp1.c` |
| **Windows 框架** | **GpioClx** |
| **本專案** | `windows_sources/gpio/rp1-gpio/` ｜ INF `rp1gpio.inf`（`ACPI\RPI50006`） |
| **狀態** | 🔵 邏輯完整（x64 sim 19/19） |

## 1. 三個暫存器區塊

RP1 GPIO 不是一塊暫存器，而是三塊（各 bank stride `0x4000`）：

| 區塊 | 用途 |
|------|------|
| **GPIO**（per-pin CTRL/STATUS） | funcsel（腳位多工）、中斷致能/型態、IRQ latch reset |
| **RIO**（Register I/O） | 真正的輸出值 OUT、方向 OE、輸入 IN |
| **PADS** | 電氣特性：pull up/down、input/output buffer enable、drive strength、schmitt |

> 一個「設一隻腳為輸出並拉高」其實要動到三塊：funcsel(GPIO 區)、OE+OUT(RIO 區)、buffer/pull(PADS 區)。

## 2. atomic 別名暫存器（重要硬體技巧）

RIO（與多數 RP1 區塊）提供 **SET/CLR 別名視窗**：
- 寫 `base + 0x2000(SET) + reg` → 把寫進去的位元 **set**（其他位元不動）
- 寫 `base + 0x3000(CLR) + reg` → 把寫進去的位元 **clear**

這讓你**不必 read-modify-write** 就能改單一位元——避免 RMW 過程被中斷打斷的競態。
```c
// 設第 off 隻腳為高：寫 RIO_OUT 的 SET 別名
WRITE_REGISTER_ULONG((volatile ULONG*)(BankRio(Hw,b) + RP1_RIO_OUT + RP1_SET_OFFSET), 1u<<off);
// 設為輸出：寫 RIO_OE 的 SET 別名（CLR 別名則設為輸入）
WRITE_REGISTER_ULONG((volatile ULONG*)(BankRio(Hw,b) + RP1_RIO_OE  + alias), 1u<<off);
```

## 3. GpioClx 要實作什麼

約 16 個 callback，重點幾個：

| callback | 做什麼 → 呼叫 HAL |
|----------|-------------------|
| `ConnectIoPins` | 設 funcsel=GPIO、方向、**pull bias** |
| `ReadGpioPins` / `WriteGpioPins` | 讀 RIO_IN / 寫 RIO_OUT 別名 |
| `EnableInterrupt` | 設 CTRL 的 IRQEN 型態（rising/falling/level）+ 開 PCIe INTE |
| `MaskInterrupts`/`Unmask` | 操作 per-bank INTE |
| `QueryActiveInterrupts` | 讀 INTS（哪些 pin 觸發） |
| `ClearActiveInterrupts` | 寫 CTRL 的 IRQRESET latch reset |

```c
// ConnectIoPins：把 GpioClx 的 PullConfiguration 對應到 RP1 PADS bias
ULONG pud = (PullUp ? RP1_PUD_UP : PullDown ? RP1_PUD_DOWN : RP1_PUD_OFF);
Rp1GpioSelectGpioFunction(&ctx->Hw, pin);   // funcsel=GPIO + OUTOVER/OEOVER=PERI + buffer enable
Rp1GpioSetPull(&ctx->Hw, pin, pud);         // PADS PULL 欄位
Rp1GpioSetDirection(&ctx->Hw, pin, output); // RIO OE 別名
```

## 4. 中斷怎麼接回 rp1bus

GPIO 是中斷控制器：每隻腳可產生中斷。RP1 GPIO 的中斷經 per-bank **PCIe INTE/INTS**
（`0x011c`/`0x0124`）彙整，再走 RP1 內部 IRQ（bank 0/1/2）→ MSI-X → `rp1bus` demux
（見 [第 05 章](05-pcie-and-rp1-enumeration.md)）。

## 5. Pi5 實測 / B 類修正

- **B 類補正**：原本 `ConnectIoPins` 把 GpioClx 的 PullConfiguration **丟掉了**（PADS 區從沒被寫）→
  按鈕等輸入腳會浮動誤觸發。源碼 `rp1_pull_config_set`/`rp1_input_enable` 都有寫，補回。
- 另補：`SelectGpioFunction` 清 OUTOVER/OEOVER 成 PERI（否則殘留 override 會讓 RIO 方向失效）。

## 6. 踩雷

- 別忘了 **PADS 區**：只設 funcsel+方向不夠，pull/buffer 在 PADS。
- `Rp1GpioPinToBank()`：54 隻腳分 3 bank，pin→(bank, 偏移) 的對應要對。
- 中斷 latch reset（IRQRESET）與型態切換順序：切型態時先 reset 避免吃到假中斷。

➡️ 回 [目錄](README.md)
