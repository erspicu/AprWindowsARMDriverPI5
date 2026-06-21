# WiFi 移植：可落地實作指南

> 把 WHD 接進 Windows kernel-mode 的具體做法。前置策略見 [`01-strategy-and-references.md`](01-strategy-and-references.md)。

## 0. 分層架構 + 資料流

```text
[ Windows 網路堆疊 TCPIP.sys ]
        ↓
[ NetAdapterCx (微軟提供) ]
        ↓  EvtPacketQueueAdvance / NetRxQueueNotifyMoreAdvanced   [IRQL ≤ DISPATCH]
[ 你的 KMDF 驅動 Pi5CywWifi.sys ]
   - WDF 裝置初始化、ACPI/OOB 中斷解析
   - Rx/Tx Ring Buffer（同步層）
   - PsCreateSystemThread 建 WWD thread
        ↓  KeWaitForSingleObject / KeSetEvent   [★ PASSIVE_LEVEL 絕對邊界 ★]
[ WHD Core（純 C library，靜態連結進 .sys）]
   - WWD Thread（無限迴圈，跑 WiFi 狀態機）
   - whd_rtos_win.c（KeInitializeSemaphore…）
        ↓  cyhal_sdio_send_cmd / cyhal_sdio_bulk_transfer
[ WHD SDIO HAL：cyhal_sdio_win.c ]
   - 建 IRP、封 SDBUS_REQUEST_PACKET
        ↓  SdBusSubmitRequest
[ sdbus.sys (Windows 內建 SD 匯流排) ]
        ↓
[ Pi5 Broadcom SD Host 驅動 ] → 硬體 CMD/DAT
```

> **黃金規則**：WHD 一律在 **PASSIVE_LEVEL** 跑（SDIO IRP 是 blocking）。NetAdapterCx 的 Rx/Tx callback 可能在
> **DISPATCH_LEVEL** → **絕不可在 DISPATCH 呼叫 WHD 的 blocking API**。中間一定隔一層 **Ring Buffer + KEVENT**。

## 1. WHD 接進 kernel：實作 `whd_rtos_win.c`（OS 抽象層）

WHD 預期 RTOS（threads/semaphores/mutexes/timers）。對應到 Windows kernel：

| WHD `cy_rtos_*` | Windows kernel API |
|------------------|--------------------|
| `cy_rtos_create_thread` | `PsCreateSystemThread`（一條 PASSIVE_LEVEL 系統執行緒 = WWD thread）|
| `cy_rtos_init_semaphore` | `KeInitializeSemaphore(sem, count, limit)` |
| `cy_rtos_get_semaphore` | `KeWaitForSingleObject(sem, Executive, KernelMode, FALSE, &timeout)`（ms → 100ns 單位，負值）|
| `cy_rtos_set_semaphore` | `KeReleaseSemaphore(sem, 0, 1, FALSE)` |
| `cy_rtos_init_mutex` | `KeInitializeMutex` |
| `cy_rtos_delay_milliseconds` | `KeDelayExecutionThread(KernelMode, FALSE, &li100ns)` |
| `cy_rtos_malloc` | `ExAllocatePool2(POOL_FLAG_NON_PAGED, size, 'DHWw')`（**必須 NonPaged**，SDIO DMA/高 IRQL 會碰）|

> **直接抄 Zephyr 的 `cybsp_wifi_os.c`**，把 Zephyr API 換成上表的 Windows API 即可。

## 2. WHD SDIO HAL：實作 `cyhal_sdio_win.c`

要實作的 callback：
- `cyhal_sdio_init`：發 `IRP_MN_QUERY_INTERFACE` 向 SD Host 取 `GUID_SDBUS_INTERFACE_STANDARD`，拿到 `SdBusSubmitRequest` 函式指標。
- `cyhal_sdio_send_cmd`（CMD52）/`cyhal_sdio_bulk_transfer`（CMD53 block mode）：封 `SDBUS_REQUEST_PACKET` 呼叫 SdBus。
- `cyhal_sdio_irq_enable`：In-band 用 sdbus callback；OOB 用 GPIO 中斷（見 §4）。

## 3. SDIO function driver（KMDF）+ CMD52/53

**掛載**：INF match SDIO ID（如 `SD\VID_02D0&PID_A9A6`）；`EvtDeviceAdd` 用 `WdfFdoQueryForInterface(&GUID_SDBUS_INTERFACE_STANDARD, …)` 取得 SdBus 介面。

**CMD52（單暫存器讀寫）骨架**：
```c
NTSTATUS SdioCmd52(PSDBUS_INTERFACE_STANDARD SdBus, UCHAR Func, ULONG Addr, UCHAR *Data, BOOLEAN Write) {
    SDBUS_REQUEST_PACKET req = {0};
    req.RequestFunction = Write ? SDBUS_REQUEST_TYPE_WRITE_REG : SDBUS_REQUEST_TYPE_READ_REG;
    req.Parameters.DeviceCommand.Function = Func;
    req.Parameters.DeviceCommand.Address  = Addr;
    if (Write) req.Parameters.DeviceCommand.Data = *Data;

    PIRP Irp = IoAllocateIrp(StackSize, FALSE);     // SdBusSubmitRequest 需要 IRP
    // 設 IoCompletionRoutine：完成時 KeSetEvent
    NTSTATUS st = SdBus->SubmitRequest(SdBus->Context, &req, Irp);
    // KeWaitForSingleObject 等 IRP 完成（把非同步轉成 WHD 要的同步）
    if (!Write) *Data = req.Parameters.DeviceCommand.Data;
    return st;
}
// CMD53：RequestFunction 用 SDBUS_REQUEST_TYPE_READ/WRITE_MULTI_BLOCK，填 Length + Buffer
```
> **陷阱**：`SdBusSubmitRequest` 是非同步（回 STATUS_PENDING），WHD 預期同步 → 你要自己 alloc IRP + completion routine + `KeWaitForSingleObject` 阻塞當前 thread。

**In-band interrupt**：拿到介面後呼叫其 `InitializeInterface` 傳入 `PSDBUS_CALLBACK_ROUTINE`；DAT1 拉低時 sdbus 呼叫你的 callback → `KeSetEvent` 喚醒 WWD thread → 處理完發 `SDBUS_REQUEST_TYPE_ACKNOWLEDGE_INT` 重新 arm。

**SD host 能力**：理論上 `sdbus.sys` 列舉時自動切 1.8V + High-Speed(50MHz)；用 `SDBUS_REQUEST_TYPE_GET_PROPERTY` 讀 `SDBUS_PROPERTY_BLOCK_SIZE` 對齊 WHD buffer。（**此處需實機試**，Pi5 Broadcom SD host 在 Windows 可能有雷。）

## 4. OOB GPIO 中斷 + ACPI

**ASL**（OOB IRQ 掛在 SDIO function 子裝置，不是 host）：
```asl
Device (BRC3) {                         // SD Host 下的 WiFi 子裝置
    Name (_ADR, 1)                      // SDIO Function 1
    Method (_CRS, 0, NotSerialized) {
        Name (RBUF, ResourceTemplate () {
            // WL_HOST_WAKE → Pi5 的某根 GPIO
            GpioInt (Edge, ActiveHigh, ExclusiveAndWake, PullNone, 0, "\\_SB.GPI0") { <PIN> }
        })
        Return (RBUF)
    }
}
```
**連接**：`EvtDevicePrepareHardware` 找 `CmResourceTypeInterrupt` → `WdfInterruptCreate` → `EvtInterruptIsr` 內 `WdfInterruptQueueDpcForIsr` → DPC 內 `KeSetEvent` 喚醒 WWD thread。

**In-band vs OOB（重要決策）**：
- **MVP 先用 Polling**：WHD 支援純 polling（WWD thread 裡一直讀狀態暫存器）。Windows `sdbus.sys` 的 in-band
  中斷延遲高、Broadcom 晶片上易漏中斷。**強烈建議先 polling 確認資料打得通，再接 OOB GPIO**，別死磕 in-band。

## 5. NetAdapterCx 資料路徑（最易出 IRQL bug）

**Rx（WHD → Windows）**：
```c
// WHD thread 讀到一包 → whd_network_process_ethernet_data → 放進你的 Ring Buffer
// → NetRxQueueNotifyMoreAdvanced 通知 NetAdapterCx
// Windows 之後呼叫 EvtPacketQueueAdvance(RxQueue)：
while (NetRingHasUnfragmentedPackets(pr)) {
    UINT32 idx = pr->NextIndex;
    NET_FRAGMENT *frag = NetRingGetFragmentAtIndex(fr, idx);
    // RtlCopyMemory 從你的 buffer 複製到 frag->OsReturnedBuffer
    frag->ValidLength = pkt_len; frag->Offset = 0;
    pr->NextIndex = NetRingIncrementIndex(pr, idx);
}
```
**Tx（Windows → WHD）**：
```c
// EvtPacketQueueAdvance(TxQueue)：
while (NetRingHasElements(pr)) {
    NET_FRAGMENT *frag = NetRingGetFragmentAtIndex(fr, pr->NextIndex);
    // 1. whd_host_buffer_get() 取 WHD buffer
    // 2. RtlCopyMemory 複製 frag->OsReturnedBuffer 進去
    // 3. 把指標丟進 WWD worker thread 的 queue
    pr->NextIndex = NetRingIncrementIndex(pr, pr->NextIndex);
}
KeSetEvent(&TxNotifyEvent, 0, FALSE);   // 喚醒 WWD thread 去送
```
**同步**：Rx/Tx Advance 在 ≤DISPATCH、WWD thread 在 PASSIVE → **中間一定隔 Queue + KEVENT**；**MVP 別做 zero-copy，
用 `RtlCopyMemory` 最穩**。

## 6. Bring-up 順序（照這個做）

1. **空殼 driver**：能掛到 SDIO 上的 KMDF FDO，證明綁定成功。
2. **SDBUS 測試**：寫 `cyhal_sdio_win.c`，DriverEntry 發 CMD52 讀 CYW43455 backplane 暫存器 →
   **讀到 Chip ID `0x4345` = 這條路通了 80%**。← 第一個關鍵里程碑
3. **WHD 整合**：編 WHD 進來、實作 thread/semaphore、PASSIVE_LEVEL 啟動 `whd_wifi_on`、**Polling Mode** 確認能跟韌體溝通並載入 `.bin`。
4. **NetAdapterCx 整合**：最後才掛虛擬網卡 Rx/Tx queue + buffer 拷貝。

➡️ 陷阱與待確認項見 [`03-pitfalls-and-open-questions.md`](03-pitfalls-and-open-questions.md)。
