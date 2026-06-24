# 藍牙（CYW43455/BCM4345 UART HCI）實作規劃

> 路線見 [`MD/Note/bluetooth/`](../../../MD/Note/bluetooth/)。本檔是**逐步執行清單**。
> 圖例：☐ 待做 / ☑ 完成｜**[x64]** 可在本機 sim 驗證 → 推到 🔵｜**[Pi5]** 需實機 → 推到 ✅。
> 現況：KMDF 骨架 + H4 command framing + BCM init 表 + sim 綠燈（🟢）。

## 架構回顧
KMDF **Bluetooth Extensible Transport Driver**（`bthx.h`）夾在 inbox `bthport.sys`（上）與 UART（下）之間。
只做 H4 byte-stream 搬運 + BCM vendor init（.hcd + baud 切換）。**`bthx.h` 在標準 WDK shared/**（更新 common.h 註記）。

---

## Phase A — 純邏輯（x64 sim 可全驗，推 🔵）

- ☑ **A0** H4 command framing（`BtBcmBuildCommand`）+ BCM init 表 + sim。*(已存在)*
- ☑ **A1 [x64] H4 RX 解析狀態機**（`h4_parser.c`）：byte-stream → 完整 HCI packet。**sim 22/22 過。**
  - 依型別表：`0x01`Cmd(hdr3,len@2,1B) / `0x02`ACL(hdr4,len@2-3,2B LE) / `0x03`SCO(hdr3,len@2,1B) / `0x04`Evt(hdr2,len@1,1B)。
  - 狀態：`WANT_TYPE→WANT_HEADER→WANT_PAYLOAD→完成`；支援分段餵入；含 `H4IsCommandComplete`。
  - 已驗：整包/逐 byte 分段/兩包黏連/ACL 2B 長度跨界/未知 byte resync/command-complete 判讀。
- ☑ **A2 [x64] BCM vendor payload 產生器**（`hci.c`）：**sim 過，用 Pi5 實機值驗證。**
  - `BtBcmBuildBaudRatePayload`：`00 00 + baud LE32`（3M=`00 00 C0 C6 2D 00`，= Pi5 DT max-speed ✓）。
  - `BtBcmBuildBdAddrPayload`：6-byte 反序 MAC（Pi5 88:a2:9e:58:5f:d6 → `d6 5f 58 9e a2 88` = DT local-bd-address ✓）。
- ☑ **A3 [x64] .hcd record 解析器**（`BtBcmHcdNextCommand`）：**sim 過，格式經 Pi5 BCM4345C0.hcd 確認。**
  - `[opcode2 LE][len1][data]` → 加 H4 `0x01` → HCI command；回傳 frame 長度 + consumed；含截斷/結尾判斷。
  - 真機驗證：首筆 = `4c fc 46 ...`（Write_RAM 0xFC4C, len 0x46）。
- ☑ **A4 [x64] init 狀態機**（`init_sm.c`：`BtBcmInitStart`/`BtBcmInitOnEvent`）：**sim 過。**
  - 串成 Reset→Download→loop .hcd(Write_RAM)→Launch_RAM→Reset→Write_BD_ADDR→Update_Baud_Rate，每步用 `H4IsCommandComplete` 判斷再進下一步；transport 用 `BTI_TX_FN` 注入（可測）。
  - 驗：mock chip 回 complete → 走完 8 步且 opcode 順序符合 bcm_setup；status≠0 → Error。

> **Phase A 全完成（A0-A4）→ BT 達 🔵（狀態機全移植 + x64 sim 35/35 過）。** 下一步進 Phase B（WDF 接 UART/bthx，需 WDK 編譯）。

## ★ 上層整合策略更新（2026-06-25，問 Gemini + 查 WDK）
> **`bthx.h` 不在現代公開 WDK**（Win8 IHV 專屬，Win10 後移除；`IOCTL_BTHX_*` hex 未公開，要反組譯 `bthport.sys`）。
> **更好的路：Windows 內建 `BthUart.sys`**（原生序列 H4 HCI transport）。標準 H4 UART BT **不必寫 BTHX**——
> 用 **ACPI 把 BT 掛在 UART 下 + INF（`Include=bth.inf`/`Needs=BthUart.NT`）**，inbox stack 接管 bthport。
> 我們只需處理**非標準的 BCM `.hcd` Patch RAM 韌體載入 + baud 切換**（＝已完成的 Phase A 邏輯 + uart.c bring-up），
> 當成 **BthUart 的 vendor 行為 / 一個薄 filter**。→ B4 從「自寫 BTHX」改為「BthUart INF + BCM 韌體 filter」。

## Phase B — WDF / 硬體接線（需 WDK 編譯；部分要 Pi5）

- ☑ **B1 [x64-build] 取資源 + 開 UART/GPIO IoTarget**（`uart.c` `BtBcmParseResources`/`BtBcmOpenTargets`，接 `driver.c` `EvtDevicePrepareHardware`）：解析 `CmResourceTypeConnection`(SERIAL/GPIO) → `RESOURCE_HUB_CREATE_PATH_FROM_ID`（需 `#define RESHUB_USE_HELPER_ROUTINES`）→ `WdfIoTargetOpen`。**ARM64 /kernel 編譯乾淨。**
- ☑ **B2 [x64-build] init 期同步收發 + bring-up**（`uart.c` `BtBcmBringUp`）：拉高 BT_REG_ON、UART 115200、用 init_sm（A4）跑 bring-up（`WdfIoTargetSendWriteSynchronously` Tx + `...ReadSynchronously` 餵 A1 parser → 推進狀態機）、完成後 host 切 3M baud + purge。**ARM64 /kernel 乾淨。**（功能待 Win11 實機）
- ☐ **B3 [x64-build] 常駐 RX read pump**（運行期）：async `WdfIoTargetFormatRequestForRead`+completion 串接，餵 A1 parser → 交給上層。
- ☐ **B4 [改] 上層整合走 BthUart**：寫 INF `Class=Bluetooth`+ClassGuid + `Include=bth.inf`/`Needs=BthUart.NT(.Services)`；BCM `.hcd` 載入（`ZwReadFile`）+ bring-up 當 vendor/filter 行為。（不再自寫 BTHX IOCTL；若 BthUart 不支援 BCM patchram 才退回自訂 filter）
- ☐ **B5 .hcd 載入**：`ZwReadFile` 讀 `\SystemRoot\System32\drivers\BCM4345C0.hcd` → 餵 `BtBcmBringUp`。

## Phase C — 實機（Pi5）

- ☐ **C1 [Pi5] ACPI DSDT**：BT device（UartSerialBusV2 指 PL011 + GpioIo/GpioInt）。**Pi5 BT 接哪個 PL011 + GPIO pin 需實機查**。
- ☐ **C2 [Pi5] M2 晶片心跳**：BT_REG_ON↑ → 送 HCI_Reset → 收 `04 0E 04 01 03 0C 00`（晶片活、UART 通）。← **第一實機里程碑**
- ☐ **C3 [Pi5] M3 韌體**：取得 `BCM4345C0.hcd`，跑 A3/A4 完整載入 + baud 切換（host `IOCTL_SERIAL_SET_BAUD_RATE`）。
- ☐ **C4 [Pi5] M4 bthport 疊合**：裝置管理員出現 "Bluetooth Radio"。
- ☐ **C5 [Pi5] M5**：配對滑鼠/鍵盤能用。電源管理（D0Exit/HOST_WAKE arm-for-wake）。

---

## 下一步（本機可立刻做）
**A1 H4 RX parser + sim** → 接著 A2/A3/A4。全部走 `windows_sources/bluetooth/bcm43438/sim/` 的 `BTBCM_SIM` x64 測試，
過了就把 BT 從 🟢 推到 🔵（狀態機全移植 + sim 過）。
