# B9：RP1 I2C ×7（Synopsys DesignWare I2C）

> 範本章：示範一個完整硬體章的寫法。RP1 的 I2C 是業界標準 **Synopsys DesignWare I2C** IP，
> 暫存器有公開文件、Linux 驅動成熟，是入門移植的好起點。

| | |
|---|---|
| **裝置** | RP1 I2C 控制器 ×7（40-pin 排針上的 I2C 走這） |
| **IP** | Synopsys DesignWare APB I2C（`snps,designware-i2c`） |
| **Linux 源碼** | `drivers/i2c/busses/i2c-designware-{common,master,platdrv}.c` |
| **Windows 框架** | **SpbCx**（Simple Peripheral Bus） |
| **本專案** | `windows_sources/i2c/rp1-dw-i2c/` ｜ INF `windows_driver/i2c/rp1i2c.inf`（`ACPI\RPI50001`） |
| **狀態** | 🔵 邏輯完整（x64 sim 18/18），剩實機驗證 |

## 1. 裝置概述

I2C 是兩線（SCL 時脈、SDA 資料）的低速匯流排，掛溫度感測、RTC、EEPROM、感測器等。
DesignWare I2C 是個 FIFO-based master：你設好目標位址與速度，把要送的 byte 丟進 TX FIFO，
讀回來的 byte 從 RX FIFO 撈。

## 2. 規格 / 暫存器重點

| 暫存器 | offset | 用途 |
|--------|-------:|------|
| `DW_IC_CON` | 0x00 | 主控設定（master / 7-bit / restart / speed） |
| `DW_IC_TAR` | 0x04 | 目標 slave 位址 |
| `DW_IC_DATA_CMD` | 0x10 | 寫=送資料；讀=發 read 命令 / 收資料（bit8=STOP、bit9=RESTART、bit10=READ） |
| `DW_IC_SS/FS_SCL_HCNT/LCNT` | 0x14–0x20 | SCL 高/低週期計數（**決定 bus 速度，依輸入時脈算**） |
| `DW_IC_ENABLE` | 0x6c | 啟用控制器 |
| `DW_IC_ENABLE_STATUS` | 0x9c | disable 後輪詢這裡確認真的停了 |
| `DW_IC_SDA_HOLD` | 0x7c | SDA hold time（RX-hold workaround） |
| `DW_IC_COMP_TYPE` | 0xfc | 身分驗證值 `0x44570140`（"DW"+版本） |

**速度怎麼來**：`SCL = 輸入時脈 / (HCNT + LCNT + ...)`。所以 HCNT/LCNT 要**依實際輸入時脈算**——
這是個經典「**A 類缺漏**」：源碼只有公式（`i2c_dw_scl_hcnt/lcnt`），實際時脈要量實機（見下方 §6）。

## 3. Linux 源碼參考（要抄的邏輯）

- `i2c_dw_init_master()` / `i2c_dw_configure_master()`：disable → 寫 CON → 寫 SCL counts → FIFO threshold → mask 中斷。
- `i2c_dw_set_timings_master()`：用 `i2c_dw_scl_hcnt/lcnt(ic_clk, ...)` 算 HCNT/LCNT。
- `i2c_dw_xfer_msg()`：把 byte 寫進 `DATA_CMD`，最後一個帶 STOP；讀則發 read 命令再撈 RX FIFO。
- `__i2c_dw_disable()`：寫 ENABLE=0 後**輪詢 ENABLE_STATUS** 直到真的停。

## 4. Windows 框架對應（SpbCx）

SpbCx 幫你扛「I2C 傳輸排程」，你補「跟硬體講話」：
- `EvtSpbTargetConnect`：拿到 slave 位址 → 呼叫 HAL `SetTarget`。
- `EvtSpbControllerLock/Unlock`：序列化存取。
- `EvtSpbIoRead/Write`：把 SPB request 翻成 HAL 的 `WritePolled`/`ReadPolled`。

## 5. Code sample（本專案 HAL，對照 Linux 重寫）

`windows_sources/i2c/rp1-dw-i2c/dw_i2c_hw.c`：
```c
// 對照 i2c_dw_configure_master()
VOID DwI2cHwConfigureMaster(PDW_I2C_HW Hw, DW_I2C_SPEED Speed)
{
    Hw->MasterCfg = DW_IC_CON_MASTER | DW_IC_CON_SLAVE_DISABLE |
                    DW_IC_CON_RESTART_EN | SpeedBits(Speed);
    DwI2cHwDisable(Hw);
    DwI2cWrite(Hw, DW_IC_CON, Hw->MasterCfg);

    // SCL 高低計數：依「實測 clk_sys = 200 MHz」用 DesignWare 公式算（見 §6）
    DwI2cWrite(Hw, DW_IC_SS_SCL_HCNT, 0x0359);   // 857  (100 kHz)
    DwI2cWrite(Hw, DW_IC_SS_SCL_LCNT, 0x03E7);   // 999
    DwI2cWrite(Hw, DW_IC_FS_SCL_HCNT, 0x00B1);   // 177  (400 kHz)
    DwI2cWrite(Hw, DW_IC_FS_SCL_LCNT, 0x013F);   // 319

    // SDA hold time：版本 ≥1.11 的 RX-hold workaround（避免 TX arbitration-lost）
    UINT32 hold = DwI2cRead(Hw, DW_IC_SDA_HOLD);
    if (!(hold & DW_IC_SDA_HOLD_RX_MASK)) hold |= 1u << 16;
    DwI2cWrite(Hw, DW_IC_SDA_HOLD, hold);

    DwI2cWrite(Hw, DW_IC_INTR_MASK, 0);          // polled 模式
}

// 對照 __i2c_dw_disable()：寫 0 後要輪詢確認真的停
VOID DwI2cHwDisable(PDW_I2C_HW Hw)
{
    DwI2cWrite(Hw, DW_IC_ENABLE, 0);
    for (UINT32 i = 0; i < 100; i++)
        if (!(DwI2cRead(Hw, DW_IC_ENABLE_STATUS) & DW_IC_ENABLE_STATUS_IC_EN)) break;
}
```

傳一個 byte（寫）：
```c
// 最後一個 byte 帶 STOP；其餘只寫資料
DwI2cWrite(Hw, DW_IC_DATA_CMD, byte | (last ? DW_IC_DATA_CMD_STOP : 0));
```

## 6. Pi5 實測事實（用實機把「猜的值」變「確定」）

用 Pi5 SSH 唯讀量到（見 [第 08 章](08-hardware-truth-and-ab-gaps.md)）：
- RP1 I2C 的輸入時脈 = **clk_sys = 200 MHz**（`/sys/kernel/debug/clk/clk_summary`）。
- 用 DesignWare 公式 `i2c_dw_scl_hcnt/lcnt`（ic_clk=200000 kHz、tf=300ns）算出：
  - SS(100k)：HCNT=857(0x359)、LCNT=999(0x3E7)
  - FS(400k)：HCNT=177(0xB1)、LCNT=319(0x13F)
- **這修正了一個 bug**：原本照「~85 MHz」估的值會讓 bus 跑快 2.3 倍（100k→235k）。

> 教學點：**源碼有公式（B 類可抄），但公式的輸入（時脈）是 A 類，得量實機才知道是 200 MHz**——
> 兩者對上，HCNT/LCNT 才敢寫死。這就是「規格缺漏 A/B 兩類」的活教材。

## 7. x64 模擬驗證

`sim/dwi2c_sim.c` mock 了 COMP_TYPE、STATUS、TX/RX FIFO，斷言整條 configure→setTarget→write→read 序列：
```
== DesignWare I2C transfer-engine simulation ==
  [PASS] MasterCfg = MASTER|SLAVE_DIS|RESTART|FAST
  [PASS] SDA_HOLD RX-hold workaround set (bit16)
  [PASS] byte0 == 0xAB, no STOP   ... [PASS] byte1 == 0xCD, with STOP
  [PASS] rbuf[0] == 0x11   [PASS] rbuf[1] == 0x22
== 18 passed, 0 failed ==
```
→ 不需硬體就確認「狀態機正確」= 🔵 邏輯完整。

## 8. 移植要點 / 踩雷

- **HCNT/LCNT 一定要依實際時脈算**（別照別的平台抄）。
- **disable 要輪詢 ENABLE_STATUS**：否則 `SetTarget` 的 disable→寫 TAR→enable 可能在控制器還 busy 時被忽略 → 偶發卡死。
- **SDA_HOLD RX-hold workaround**：版本 ≥1.11（RP1 是 0x140）要設，避免 slave 太快拉低 SDA 造成 TX arbitration-lost。
- BCM2712 上另有一套 **BSC I2C**（`i2c-bcm2835`，非 DesignWare），時脈 **108 MHz**、分頻方式不同——別跟 RP1 這套混。

## 對照延伸

- 同為 DesignWare 的 SPI（B8）、I2S（B14）寫法雷同。
- BCM2712 BSC I2C（A8）是另一套 IP（時脈/分頻不同），可比較差異。

➡️ 回 [目錄](README.md)
