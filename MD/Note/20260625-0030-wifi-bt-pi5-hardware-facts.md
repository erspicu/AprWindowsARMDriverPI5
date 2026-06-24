# Pi5 實機萃取：WiFi / 藍牙 硬體真相（2026-06-25）

> 來源：SSH 唯讀萃取 Pi5 Linux（kernel 6.18，Debian 13）。用來校正 WiFi/BT 移植實作。
> 連線法見 [`MD/Skill/pi5-ssh-hardware-facts.md`](../Skill/pi5-ssh-hardware-facts.md)。

## WiFi（CYW43455 / BCM4345/6，SDIO）

| 事實 | 值 | 驗證了什麼 |
|------|-----|-----------|
| **backplane F1 signature** | `read @0x18000000 = 0x15264345` | ✅ **驗證 `Cyw43455ReadChipId`**：chip-id 暫存器在 **0x18000000**，低 16 bit = **0x4345**（高 16=0x1526 rev/magic）|
| chip | BCM4345/6（dmesg）| |
| SDIO host | `mmc1`，UHS-I **DDR50** SDIO card @ address 0001 | Pi5 SDIO host |
| 韌體 bin | `brcm/brcmfmac43455-sdio.bin` → `cypress/cyfmac43455-sdio.bin` | Phase B7 嵌入用 |
| clm_blob | `cyfmac43455-sdio.clm_blob` | |
| **nvram（Pi5）** | `brcm/brcmfmac43455-sdio.txt`（2074 bytes，82 個 key）| ✅ **驗證 `WhdNvramPreprocess`**：真檔 → 1743 bytes、無 `\n`/`#`、雙 NUL 結尾、macaddr= 在 |
| FW 版本 | 7.45.265（2023-08-29）| |

> nvram 已拉到 `temp/pi5_nvram_43455.txt`（gitignore，不入版控；內含真實 macaddr，屬個資）。
> SBADDR window 確切 bit layout 仍待對 brcmfmac `bcmsdh.c` 確認；但 chip-id target/value 已實機驗證。

## 藍牙（CYW43455 BT，UART HCI）

| 事實 | 值 | 驗證了什麼 |
|------|-----|-----------|
| 介面 | **HCI UART, Broadcom 協定 H4** | ✅ 路線正確 |
| **UART** | `serial@7d50c000`（**BCM2712 SoC 的 PL011**，非 RP1），compatible **`brcm,bcm7271-uart`** | ACPI `UartSerialBusV2` 要指這顆 |
| BT DT compatible | **`brcm,bcm43438-bt`** | 對上專案目錄 `bcm43438` |
| **max-speed** | `00 2d c6 c0`(BE) = **3,000,000 baud** | ✅ 驗證 `OperBaud=3000000` |
| **shutdown-gpios（BT_REG_ON）** | phandle 0x1b, **GPIO #29**, flags 0(active-high) | ACPI GpioIo 用 |
| **local-bd-address** | `d6 5f 58 9e a2 88`（= 反序的 88:a2:9e:58:5f:d6）| ✅ 驗證 `Write_BD_ADDR` 6-byte **反序** 格式 |
| chip | **BCM4345C0**（chip id 107, ver 003.001.025, features 0x2f）| |
| patch（.hcd）| `brcm/BCM4345C0.raspberrypi,5-model-b.hcd` → `BCM4345C0.hcd`（63806 bytes）| ✅ 檔名確認 |
| **.hcd 格式** | 首筆 = `4c fc 46 00 ...` → opcode **0xFC4C(Write_RAM)** + len 0x46 + data | ✅ **驗證 `[opcode2 LE][len1][data]`**（A3 parser 設計）|
| **ACL MTU** | **1021**:8（hciconfig）| ✅ 驗證 bthx `MaxAclTransferInSize≈1021` |
| SCO MTU | 64:1 | |

> BT_HOST_WAKE（OOB）：DT bluetooth node **只有 shutdown-gpios**，無 host-wake gpio → Pi5 BT 用 in-band，
> 與筆記「MVP 先 polling/in-band、OOB 非必須」一致。

## 對實作的影響（已套用 / 待套用）
- ✅ WiFi A1（chip-id 0x4345）、A2（nvram 預處理）— **實機數據驗證通過**。
- ✅ BT：max-speed 3M、BD_ADDR 反序、.hcd 格式 — 確認，餵進 A2/A3 實作。
- 待：BT ACPI 用 `brcm,bcm7271-uart`@7d50c000 + GPIO29；WiFi SBADDR 細節對 brcmfmac。

## BT 上層整合策略更新（問 Gemini + 查 WDK，2026-06-25）
- **`bthx.h` 不在現代公開 WDK**（10.0.26100/22621/19041 的 km、shared 都沒有）。它是 **Win8 時代 IHV/BSP 專屬**，Win10 後從一般 WDK 移除。struct 欄位 MS Learn 有文件，但 **`IOCTL_BTHX_*` 的 hex code 未公開**（Gemini 拒絕亂猜，建議反組譯 `bthport.sys` 取值）。
- **✅ 更好的路：inbox `BthUart.sys`**（Windows 10/11 內建的原生序列 H4 HCI transport）。標準 H4 UART 藍牙**不必寫 BTHX driver**：
  - ACPI 把 BT 掛在 UART 下（`brcm,bcm43438-bt` 已是 Linux compatible；Windows 端給 `_HID` + UartSerialBusV2）。
  - INF：`Class=Bluetooth` + `Include=bth.inf` / `Needs=BthUart.NT(.Services)` → inbox stack 接管 bthport。
  - **只剩非標準的 BCM `.hcd` Patch RAM 韌體載入 + baud 切換**要我們做（= 已完成的 Phase A 邏輯 + `uart.c` bring-up），當 vendor 行為 / 薄 filter。
- WDK 其他相關 header 確認：`reshub.h`✅（`RESOURCE_HUB_CREATE_PATH_FROM_ID` 需 `#define RESHUB_USE_HELPER_ROUTINES` + `NTDDI_VERSION>=WIN8`）、`gpio.h`✅(shared)、`ntddser.h`✅。
