# 藍牙移植：架構決策 + 開源參考

## 1. 架構：Bluetooth Extensible Transport Driver

```
[ Windows 藍牙 stack：bthport.sys（L2CAP/SDP/配對/profiles）]   ← inbox，全包
        ↕  IOCTL_BTHX_READ_HCI / WRITE_HCI（HCI packet 在 SystemBuffer）
[ 你的 KMDF Transport Driver（bthx.h 介面）]                    ← 你只寫這個
   - 只做：H4 framing（加/拆 0x01/0x02/0x04 type byte）+ BCM vendor init
        ↕  UART（H4 byte stream）
[ SerCx2 / serial.sys → PL011 UART ] + GPIO（BT_REG_ON / BT_HOST_WAKE）
```

- **你不做**：L2CAP、SDP、配對、profiles → `bthport.sys` 全包。
- **你只做**：把 bthport 的 HCI payload 加 H4 type 寫 UART；從 UART 讀 H4 byte stream 組回 HCI packet 交給 bthport；
  以及開機時的 **BCM vendor init**（韌體 .hcd 載入 + baud 切換），且在「呈現標準 HCI 給 bthport 之前」私下做完。

### devnode 疊法
```
[ACPI\<BT _HID>] → 底層 UART bus 提供 UART resource
   → [你的 KMDF Transport Driver (FDO)]   ← 你
   → [bthport.sys (Upper Filter)]
   → [bthenum.sys]
```
- **沒有好用的 inbox UART-HCI transport**（不像 Linux 的 hci_uart / hciattach）——**整個 transport 要自己寫**。
- `bthmini` 是 XP/7 舊架構，**已棄用、忽略**。

## 2. 開源參考 / SOURCES

### Windows 框架側
| 資源 | 用途 |
|------|------|
| **Bluetooth Extensible Transport Guidelines**（MS Learn）| BTHX 介面規範（`IOCTL_BTHX_*`）。https://learn.microsoft.com/en-us/windows-hardware/drivers/bluetooth/bluetooth-extensible-transport-guidelines |
| `bthx.h`（**標準 WDK**）| `…\Windows Kits\10\Include\<ver>\shared\bthx.h`，定義所有 BTHX IOCTL/struct，**ARM64 支援** |
| `Windows-driver-samples` | **沒有開源 transport sample**（藍牙範例多是 profile：`bthecho`/`bthle`）；可參考 `serial/serenum` 學 UART stack 串接 |

### 晶片 / 韌體側（Linux 參考）
| 資源 | 用途 |
|------|------|
| Linux `drivers/bluetooth/hci_bcm.c` + `hci_h4.c` | **BCM init 流程 + vendor opcodes + H4** 的最佳參考 |
| BlueZ `tools/hciattach_bcm.c` | **最清晰的 .hcd parsing + baud 協商**邏輯 |
| **.hcd 韌體**：`RPi-Distro/bluez-firmware` | https://github.com/RPi-Distro/bluez-firmware/tree/master/broadcom →（Pi5 CYW43455 用 **`BCM4345C0.hcd`**）。可直接複製到 Windows 用，無須轉換 |

> 註：Pi5 BT 晶片有時標 Synaptics（Broadcom IoT 部門被收購），但韌體架構/檔名仍是 `BCM4345C0`。

## 3. 工程量 / 風險

- **~1-1.5 人月**（熟 KMDF），3000-5000 行。
- **最大痛點**：
  1. **ACPI DSDT 配合**——要在 ACPI 宣告 BT device + 正確的 `UartSerialBusV2` + `GpioIo`/`GpioInt`（可能要改 Pi5 UEFI/自寫 ASL）。**ACPI 寫錯 → driver 連 `EvtDevicePrepareHardware` 都進不去**。
  2. **UART flow control / 效能**——A2DP 等高頻寬下 PL011 沒做好 RTS/CTS/DMA 會 buffer overrun → 斷線。
  3. **電源管理**（S0ix / BT_HOST_WAKE 喚醒 / 重載韌體）。

➡️ 實作細節見 [`02-implementation-guide.md`](02-implementation-guide.md)。
