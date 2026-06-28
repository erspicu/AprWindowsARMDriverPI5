# 藍牙移植：可落地實作指南

> ⚠️ **路線更新（2026-06-25）**：本檔原以自寫 `bthx.h` transport driver 為路線，但 `bthx.h` 不在現代公開 WDK（Win8 IHV-only）→ **已改走 inbox `BthUart.sys`**（只做 BCM `.hcd` + baud 薄處理）。下方 bthx 相關內容為歷史記錄，**實作以 BthUart 為準**（見 `bluetooth/README.md` 頂部 + `windows_sources/bluetooth/bcm43438`）。

> 承 [`01-strategy-and-references.md`](01-strategy-and-references.md)。標註「需查 bthx.h / 需實機」處實作時釐清。

## 0. WDF 物件圖

```text
[BTHPORT.SYS]  ──IOCTL_BTHX_WRITE_HCI / READ_HCI──▶
+------------------------------------------------------------+
| KMDF Transport Driver (bthx)                               |
|  [WDFDEVICE]                                               |
|   ├ Default WDFQUEUE     : 收 Capabilities/SetVersion/Write|
|   ├ Manual WDFQUEUE      : 存 pended READ_HCI requests     |
|   ├ H4 State Machine ctx : Type→Header→Payload 重組        |
|   ├ Read Pump WDFREQUEST : UART 常駐非同步讀（completion 串接）|
|   ├ FW-load WDFWORKITEM  : D0Entry 跑 .hcd init（一次性）  |
|   ├ WDFIOTARGET (UART)   : 下緣 SerCx2/serial             |
|   └ WDFIOTARGET (GPIO) + WDFINTERRUPT(HOST_WAKE)          |
+------------------------------------------------------------+
        │ WdfIoTargetSend…              │ async read/write
        ▼                               ▼
   [SerCx2 / serial.sys / PL011 UART]  +  [GPIO controller]
```

## 1. bthx IOCTL 處理（核心）

bthport 透過 `IRP_MJ_DEVICE_CONTROL` 下發 `bthx.h` 的 IOCTL，用 Default queue 攔。

- **`IOCTL_BTHX_QUERY_CAPABILITIES`**（D0 後 bthport 立刻查）：填 `BTHX_CAPABILITIES`（`MaxAclTransferIn/OutSize`≈1021、`MaxScoTransfer…`≈255、`MaxCommandTransferSize`≈255；**確切 struct/值需查 bthx.h + 實機**）。
- **`IOCTL_BTHX_WRITE_HCI`**（bthport 要送 command/ACL）：`BTHX_HCI_READ_WRITE_CONTEXT` **不含 H4 type byte**——你 alloc 新 buffer 塞 1-byte type（command=0x01、ACL=0x02）+ payload，async 寫 UART，completion 裡 `WdfRequestComplete`。
- **`IOCTL_BTHX_READ_HCI`**（★ pended-read 模式）：bthport **預先下發 2-4 個 read IRP** 並 pend。做法：
  ```c
  case IOCTL_BTHX_READ_HCI:
      WdfRequestForwardToIoQueue(Request, ctx->PendedReadQueue);  // 先存著
  ```
  read pump 組好一個完整 HCI packet 時：
  ```c
  WdfIoQueueRetrieveNextRequest(ctx->PendedReadQueue, &req);
  // 複製 payload 進 req 的 BTHX_HCI_READ_WRITE_CONTEXT（去掉 H4 type byte，回傳長度需查 bthx.h）
  WdfRequestCompleteWithInformation(req, STATUS_SUCCESS, len);
  ```
  > **若 pended queue 空了**（UART 比 bthport 給 IRP 快）→ 必須有**軟體 ring buffer 暫存完整 packet**，否則掉包（A2DP/LE scan 時會發生）。
- `IOCTL_BTHX_SET_VERSION`：回報支援的 BTHX 版本（需查 bthx.h）。

## 2. UART read pump + H4 狀態機

H4 是 byte stream，無 frame 邊界，靠 header 的 length 欄位切。

| H4 type | header 長度 | length 欄位位移 | length 大小 | 後續讀 payload |
|---------|------------|----------------|-------------|----------------|
| 0x01 Command | 3 | offset 2 | 1 byte | `hdr[2]` |
| 0x02 ACL | 4 | offset 2-3 | **2 byte LE** | `hdr[2] \| (hdr[3]<<8)` |
| 0x03 SCO | 3 | offset 2 | 1 byte | `hdr[2]` |
| 0x04 Event | 2 | offset 1 | 1 byte | `hdr[1]` |
| 0x05 ISO (5.2+) | 4 | offset 2-3 | 2 byte LE | MVP 可忽略 |

狀態機：`WAIT_TYPE`(讀1)→`WAIT_HEADER`(依型別讀 1~3)→`WAIT_PAYLOAD`(讀 length)→完成→去 pended queue complete→回 `WAIT_TYPE`。
> read pump 用 `WdfIoTargetFormatRequestForRead` + `WdfRequestSend` 非同步 + completion 串接（**別 busy-poll**）。

## 3. BCM vendor init（D0Entry 一次性，read pump/bthport 前）

### opcode 表（16-bit，LE 送出；vendor OGF=0x3F→0xFC）
| 命令 | opcode | payload |
|------|--------|---------|
| HCI_Reset | **0x0C03** | 無（H4: `01 03 0C 00`）|
| Download_Minidriver | **0xFC2E** | 無（`01 2E FC 00`）|
| Write_RAM | 0xFC4C | 在 .hcd 內，不手組 |
| Launch_RAM | **0xFC4E** | `FF FF FF FF` |
| Update_Baud_Rate | **0xFC18** | `00 00` + baud 4-byte LE（3M=`00 00 C0 C6 2D 00`）|
| Write_BD_ADDR | **0xFC01** | MAC 6-byte **反序** |

### .hcd 格式：一連串 `[opcode 2 LE][len 1][data]`（無 H4 type）
```c
while (ptr < end) {
    uint8_t pkt[260]; pkt[0]=0x01; pkt[1]=ptr[0]; pkt[2]=ptr[1]; pkt[3]=ptr[2];
    memcpy(&pkt[4], &ptr[3], ptr[2]);
    Uart_Send(pkt, ptr[2]+4);
    Uart_WaitCommandComplete(/*opcode*/, 1000ms);   // ★ 每筆都要等 complete，否則晶片當機
    ptr += 3 + ptr[2];                               // chunk size 上限通常 252（需確認）
}
```

### 完整序列 + timing 地雷
1. 開 UART @115200、no flow control。
2. `HCI_Reset` → 等 complete。
3. `Download_Minidriver` → 等 complete → **delay 50ms**。
4. 迴圈寫 .hcd。
5. `Launch_RAM(FFFFFFFF)` → 等 complete → **delay 250-500ms**（晶片軟重啟，**baud 強制回 115200**）。
6. 再 `HCI_Reset` → 等 complete（確認新韌體醒）。
7. `Write_BD_ADDR`（Pi 多半無 OTP MAC，OS 寫入或隨機）。
8. `Update_Baud_Rate(3M)` → 等 complete（**這包用舊 115200 回**）。
9. **host 切 baud**（見下）→ **delay 10ms**。
10. 啟用 host RTS/CTS。
11. 啟動 read pump、完成 D0Entry（bthport 才開始發 IOCTL_BTHX）。

### host 切 baud（WDF UART IOCTL）
```c
WdfIoTargetSendIoctlSynchronously(Uart, …, IOCTL_SERIAL_PURGE, …);     // 先清 in-flight
SERIAL_BAUD_RATE br = { 3000000 };
WdfIoTargetSendIoctlSynchronously(Uart, …, IOCTL_SERIAL_SET_BAUD_RATE, &brDesc, …);
SERIAL_HANDFLOW hf = {…SERIAL_CTS_HANDSHAKE|SERIAL_RTS_CONTROL…};
WdfIoTargetSendIoctlSynchronously(Uart, …, IOCTL_SERIAL_SET_HANDFLOW, &hfDesc, …);
```

### 韌體檔
`BCM4345C0.hcd` 放 `\SystemRoot\System32\drivers\`；`EvtDevicePrepareHardware`（PASSIVE）用 `ZwCreateFile/ZwReadFile`
讀進 `ExAllocatePool2(POOL_FLAG_NON_PAGED,…)`。（放檔案系統優於 PE resource：韌體由 RPi 更新，不必重編 driver。）

## 4. ACPI DSDT + 資源消費

### ASL 骨架（BT device 帶 UART + 2 GPIO）
```asl
Device (BTH0) {
    Name (_HID, "BCM2E1A")            // 或自訂 "RP1BTHX0"，與 INF HardwareId 一致
    Method (_CRS) { Name (SBUF, ResourceTemplate () {
        UartSerialBusV2 (115200, DataBitsEight, StopBitsOne, 0xFC, LittleEndian,
            ParityTypeNone, FlowControlHardware, 32, 32,
            "\\_SB.PCI0.RP1.URT0", 0, ResourceConsumer, , )   // 指向 RP1 的 PL011
        GpioIo  (Shared, PullDefault, 0, 0, IoRestrictionOutputOnly,
            "\\_SB.PCI0.RP1.GPIO", 0, ResourceConsumer, , ) { 12 }   // BT_REG_ON（pin 需實機）
        GpioInt (Edge, ActiveHigh, Exclusive, PullDown, 0,
            "\\_SB.PCI0.RP1.GPIO", 0, ResourceConsumer, , ) { 13 }   // BT_HOST_WAKE
    }) Return (SBUF) } }
```
> Pi5 BT 接哪個 PL011（debug UART 還是專用）+ GPIO pin 號 **需查 Pi5 DT / 實機**。

### 取資源（`EvtDevicePrepareHardware`）
走訪 `WdfCmResourceListGetDescriptor`：
- `CmResourceTypeConnection` + `Class==CM_RESOURCE_CONNECTION_CLASS_SERIAL` → UART connection id
- `CmResourceTypeConnection` + `Class==…_GPIO` → GPIO(BT_REG_ON) connection id
- `CmResourceTypeInterrupt` → GpioInt(HOST_WAKE) → `WdfInterruptCreate`

開 target（UART/GPIO 同法）：
```c
RESOURCE_HUB_CREATE_PATH_FROM_ID(&path, id.LowPart, id.HighPart);   // <reshub.h>
WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&p, &path, GENERIC_READ|GENERIC_WRITE);
WdfIoTargetCreate(...); WdfIoTargetOpen(target, &p);
```
控制 BT_REG_ON（`<gpio.h>`，`IOCTL_GPIO_WRITE_PINS` + `GPIO_CLIENT_WRITE_PINS_MASK`）拉 high/low。

## 5. init 期同步收 HCI event（read pump 前）
載韌體時「送 command → 等它的 command complete」是同步的：
```c
// 送（同步、帶 timeout）
WdfIoTargetSendWriteSynchronously(Uart, NULL, &txDesc, NULL, &timeout, NULL);
// 讀（H4 解析）：先讀 1 byte type(應=0x04)，再讀 2 byte event header(code+len)，再讀 len bytes params
WdfIoTargetSendReadSynchronously(Uart, NULL, &rxDesc /*1*/, …);
WdfIoTargetSendReadSynchronously(Uart, NULL, &rxDesc /*2*/, …);
WdfIoTargetSendReadSynchronously(Uart, NULL, &rxDesc /*paramLen*/, …);
// eventCode==0x0E(Command Complete) 且 opcode 相符 → 成功
```
timeout 用 `WDF_REL_TIMEOUT_IN_SEC(2)`；`EvtDevicePrepareHardware`/`D0Entry` timeout 要拉長（韌體載入+重啟 1-2 秒）。

## 6. INF + build
- **build**：標準 KMDF（entry `FxDriverEntry`），**無特殊 lib**（只要 wdf/wdm）。bthx.h 在 `…\Include\<ver>\shared\bthx.h`。
- **INF**（讓 bthport 疊上來）：
```ini
[Version]
Class=Bluetooth
ClassGuid={e0cbf06c-cd8b-4647-bb8a-263b43f0f974}
[Standard.NTARM64]
%Desc%=Rp1Bthx_Device, ACPI\BCM2E1A          ; 對應 _HID
[Rp1Bthx_Device.NT.HW]
HKR,, "BthxTransport", 0x00010001, 1          ; 宣告支援 BTHX transport
HKR,, "BthxProtocol",  0x00010001, 1          ; 1 = UART/H4
; Include=bth.inf + Needs=BthUsb.NT(.HW/.Services) 帶起 BTHX stack — 確切 Needs 區塊需查 WDK
```
> Gemini 標註：UART BTHX 常「借用」`Needs=BthUsb.NT` 帶起完整 BTHX stack，再用 `BthxProtocol=1` 告訴 bthport 走 H4 而非 USB。**此處確切寫法需查 WDK 最新藍牙 INF 規範 / 實機驗證。**

➡️ 里程碑、電源、SCO、待驗證清單見 [`03-milestones-and-open-items.md`](03-milestones-and-open-items.md)。
