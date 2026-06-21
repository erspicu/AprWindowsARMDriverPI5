# B8：RP1 SPI ×8（Synopsys DesignWare SSI）

> 與 [B9 I2C](B09-rp1-designware-i2c.md) 同為 DesignWare IP、同走 **SpbCx**，但有幾個 SPI 特有的眉角：
> reset-chip 序列、TMOD 收/送方向、以及 DFS 欄位的「runtime 自適應偵測」。

| | |
|---|---|
| **裝置** | RP1 SPI 控制器 ×8（DesignWare APB SSI / PSSI 變體） |
| **Linux 源碼** | `drivers/spi/spi-dw-{core,mmio}.c` |
| **Windows 框架** | **SpbCx** |
| **本專案** | `windows_sources/spi/rp1-dw-spi/` ｜ INF `rp1spi.inf`（`ACPI\RPI50002`） |
| **狀態** | 🔵 邏輯完整（x64 sim 25/25） |

## 1. 規格 / 暫存器重點

| 暫存器 | offset | 用途 |
|--------|-------:|------|
| `CTRLR0` | 0x00 | 幀格式：DFS(資料位元數)、FRF(Motorola SPI)、SCPOL/SCPHA(mode)、TMOD(收送方向) |
| `CTRLR1` | 0x04 | RO 模式的接收幀數 NDF |
| `SSIENR` | 0x08 | 啟用（設定 CTRLR0 前必須先 disable） |
| `BAUDR` | 0x14 | SCKDV 分頻：SCLK = fssi / SCKDV（**偶數**） |
| `SER` | 0x10 | slave 選擇 |
| `IMR`/`ICR` | 0x2c/0x48 | 中斷遮罩 / 清除 |
| `DR` | 0x60 | 資料 FIFO |

## 2. 三個 SPI 特有眉角（都從源碼抄得到）

### (a) reset-chip 序列（暖開機殘留會咬人）
`dw_spi_reset_chip()`：`SSIENR=0 → IMR=0 → 讀 ICR 清中斷 → SER=0 → SSIENR=1`。
不做的話，暖開機殘留的 CS/中斷狀態會讓第一筆傳輸誤判 overflow / CS 提早 de-assert。
```c
VOID DwSpiHwResetChip(PDW_SPI_HW Hw) {
    DwSpiHwEnable(Hw, FALSE);
    DwSpiWrite(Hw, DW_SPI_IMR, 0);
    (void)DwSpiRead(Hw, DW_SPI_ICR);   // 讀一下就清
    DwSpiWrite(Hw, DW_SPI_SER, 0);
    DwSpiHwEnable(Hw, TRUE);
}
```

### (b) DFS 欄位「runtime 自適應偵測」（A 類，但自己會適應）
DesignWare 有兩種變體：DFS 在 bit[3:0]（4-bit）或 bit[20:16]（DFS32）。**源碼用「寫全 1 回讀」自我偵測**：
```c
DwSpiHwEnable(Hw, FALSE);
UINT32 saved = DwSpiRead(Hw, DW_SPI_CTRLR0);
DwSpiWrite(Hw, DW_SPI_CTRLR0, 0xffffffff);
UINT32 probe = DwSpiRead(Hw, DW_SPI_CTRLR0);   // 低 4 bit 黏不黏？
DwSpiWrite(Hw, DW_SPI_CTRLR0, saved);
Hw->DfsOffset = (probe & 0xF) ? 0 : 16;        // 不黏 → DFS32，欄位在 bit16
```
> 教學點：這是「A 類」（要看硬體回什麼），但**程式自己偵測自己適應**，所以不必預先量實機——
> 把偵測邏輯從源碼抄過來即可。後續 `CTRLR0 |= (Bits-1) << Hw->DfsOffset`。

### (c) BAUDR 分頻用實測 fssi 算
RP1 SPI 功能時脈 fssi = **200 MHz**（Pi5 實測 clk_sys）。SCKDV 須偶數、0 代表最慢(÷65536)：
```c
UINT16 DwSpiHwBaudDiv(UINT32 SclkHz) {
    UINT32 div = (200000000u + SclkHz - 1) / SclkHz;   // ceil
    if (div & 1) div++;                                // 偶數
    if (div < 2) div = 2;
    return (UINT16)div;
}
```

## 3. Windows 框架（SpbCx，同 I2C）

`EvtSpbIoRead/Write` → HAL `TransferPolled`（全雙工：填 TX FIFO、撈 RX FIFO）。
`EvtSpbTargetConnect` 拿 ConnectionSpeed → `DwSpiHwBaudDiv()` 算 SCKDV → `ConfigureMaster`。

## 4. Pi5 實測 / B 類修正

- fssi=200MHz（同 I2C 的 clk_sys）。
- **B 類補正**：原本缺 reset-chip 序列、TMOD 永遠 TR、BAUDR 寫死 16 → 補上後 sim 25/25。

## 5. 踩雷

- 設定 CTRLR0 前**一定先 SSIENR=0**（disable）。
- SCKDV **必須偶數**；0 是合法值（最慢），別把 0 硬改成預設值（呼叫端傳明確預設）。
- 純收(RO)大量資料要設 `CTRLR1 = NDF-1` + TMOD_RO；本專案 polled 全雙工 TR 路徑已可用，RO 為次要優化。

➡️ 回 [目錄](README.md)
