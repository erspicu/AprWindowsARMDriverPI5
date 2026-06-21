# A5：SDHCI（SD / eMMC，SdPort miniport）

> 第一個 **SdPort** 範例。SDHCI 是業界標準 SD Host Controller 介面，暫存器有公開 spec。
> 本章也是「資料命令要設對一整組暫存器，少一個就全失敗」的好教材。

| | |
|---|---|
| **裝置** | SD 卡 / eMMC 控制器（**Pi5 上在 BCM2712**，不是 RP1！） |
| **Linux 源碼** | `drivers/mmc/host/sdhci.c`(+`sdhci-brcmstb.c`/`sdhci-of-dwcmshc.c`) |
| **Windows 框架** | **SdPort miniport** |
| **本專案** | `windows_sources/storage/rp1-sdhci/` ｜ INF `rp1sd.inf`（`ACPI\RPI50009`） |
| **狀態** | 🔵 邏輯完整（x64 sim 29/29） |

## 1. 歸屬釐清（常見錯誤）

Pi5 的 **SD 卡與 eMMC 其實掛在 BCM2712**（兩顆：eMMC `0x1000FFF000`、SD 卡槽 `0x1001100000`，GIC 305/306），
**不是 RP1**。本專案一度把它當 RP1 子裝置（用 GpioInt + RP1 IRQ），後來用 Pi5 `/proc/iomem` + `/sys/class/mmc_host`
查清楚，改成 BCM2712 SoC 裝置（QWordMemory + GIC，見 [第 04 章](04-device-tree-to-acpi.md)）。
> 教學點：**歸屬搞錯，位址與中斷全錯**——這是「A 類」實機校正的價值。

## 2. SDHCI 命令引擎（核心觀念）

下一個命令的標準流程：
1. 等 `PRESENT_STATE` 的 CMD/DAT inhibit 清掉。
2. （資料命令）設 `BLOCK_SIZE`、`BLOCK_COUNT`、`TIMEOUT_CONTROL`、`TRANSFER_MODE`。
3. 寫 `ARGUMENT`。
4. 寫 `COMMAND`（index + 旗標：response 型態 / CRC / index / data-present）。
5. 等中斷（CMD_COMPLETE / 錯誤）。

## 3. 經典案例：資料命令要設「一整組」（B 類）

本專案原本只會送「無資料命令」（CMD0/CMD8 沒問題），一碰**資料命令（讀/寫 block）就全失敗**——
因為漏了源碼 `sdhci_send_command` 內必設的暫存器：

| 漏掉的 | 後果 | 補法 |
|--------|------|------|
| `TRANSFER_MODE`(0x0C) | 控制器不知道方向/塊數 → 所有 read/write 失敗 | `BLK_CNT_EN \| (read?READ) \| (multi?MULTI)` |
| `TIMEOUT_CONTROL`(0x2E) | 預設 0 → 每筆資料立即 Data-Timeout | 寫最大值 0x0E |
| `INT_ENABLE` 的錯誤位 | 錯誤不 latch → 上層卡死等不到事件 | enable 全部錯誤位（TIMEOUT/CRC/…） |
| `HOST_CONTROL` bus width/speed | 停在 1-bit/預設速度 | 接 IssueBusOperation 設 4BITBUS/HISPD |

```c
// SendCommand：資料命令補上 TIMEOUT + TRANSFER_MODE
if (DataPresent) {
    flags |= SDHCI_CMD_DATA;
    WR8 (Base, SDHCI_TIMEOUT_CONTROL, SDHCI_TIMEOUT_MAX);   // 0x0E
    WR16(Base, SDHCI_TRANSFER_MODE,   TransferMode);        // BLK_CNT_EN|READ|MULTI
}
WR32(Base, SDHCI_ARGUMENT, Arg);
WR16(Base, SDHCI_COMMAND, SDHCI_MAKE_CMD(CmdIndex, flags));
```
```c
// 預設中斷遮罩：一定要含錯誤位，否則錯誤永不 latch
SdhciSetIntEnable(Base, SDHCI_INT_DEFAULT_MASK);   // 完成 + 全部 CMD/DATA 錯誤位
```

## 4. 寬度存取要對（ARM64 雷）

SDHCI 暫存器寬度不一：`BLOCK_SIZE`=16-bit、`TIMEOUT_CONTROL`/`HOST_CONTROL`/`SOFTWARE_RESET`=8-bit、
`PRESENT_STATE`/`INT_STATUS`=32-bit。**用錯寬度（如對 8-bit 暫存器做 32-bit 寫）會出錯**——
要分別用 `WRITE_REGISTER_UCHAR/USHORT/ULONG`（見 [第 09 章](09-arm64-and-signing.md)）。

## 5. Windows 框架（SdPort miniport）

| callback | 對應 HAL |
|----------|----------|
| `GetSlotCapabilities` | 回報 BaseClockFrequencyKhz（=**200 MHz**，Pi5 實測 emmc2-clock）、bus width、voltage |
| `Initialize` | soft reset → set power → set clock → enable 中斷 |
| `IssueRequest` | 組 TransferMode + SendCommand |
| `IssueBusOperation` | reset / set clock / voltage / **bus width / speed** → HOST_CONTROL |
| `Interrupt` | 讀 INT_STATUS，拆 normal/error，回報事件 |

## 6. Pi5 實測

- base clock = **emmc2-clock = 200 MHz**（兩顆 mmc 共用）。
- 布局：host @+0x000(0x260B) + cfg @+0x400(0x200B)（dwcmshc/brcmstb 風格）。

## 7. 踩雷

- **歸屬**：BCM2712 不是 RP1。
- **資料命令的一整組暫存器**少一個就全失敗。
- **寬度**要對。
- set_power 依 spec 先寫 0 再寫電壓+ON。

➡️ 回 [目錄](README.md)
