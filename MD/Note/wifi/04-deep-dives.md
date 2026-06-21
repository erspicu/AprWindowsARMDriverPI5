# WiFi 移植：深度實作細節（第 3-6 輪 Gemini 追問整合）

> 承 [`02-implementation-guide.md`](02-implementation-guide.md)，把每一塊追到「函式原型 / 程式骨架 / 地雷」等級。
> 仍標註的「需查 WHD source / 需實機」處，是實作時邊讀 WHD source + Zephyr port + 真機解決的。

---

## A. WHD porting layer 確切函式清單（要你實作的）+ Zephyr 範本對照

> WHD 設計哲學：核心純 C，**靠外部注入三組抽象 + callback**。直接抄 Zephyr 的 port 改 API 最快。

### A1. RTOS 抽象 `cy_rtos_*`（Zephyr 範本：`abstraction-rtos/.../cyabs_rtos_zephyr.c`）
要實作（全跑 PASSIVE_LEVEL）：
- thread：`cy_rtos_create_thread / exit_thread / join_thread` → `PsCreateSystemThread / PsTerminateSystemThread`
- semaphore：`init/get/set/deinit_semaphore` → `KeInitializeSemaphore / KeWaitForSingleObject / KeReleaseSemaphore`
- mutex：`init/get/set/deinit_mutex` → `KeInitializeMutex / KeWaitForSingleObject / KeReleaseMutex`
- queue：`init/put/get/deinit_queue` → `LIST_ENTRY + KSPIN_LOCK + KSEMAPHORE` 自刻 message queue
- timer：`init/start/stop/deinit_timer` → `KTIMER + KDPC / KeSetTimer`
- delay/time：`cy_rtos_delay_milliseconds` → `KeDelayExecutionThread`；`cy_rtos_get_time` → `KeQueryTickCount`
- malloc：`#define` 導到 `ExAllocatePool2(POOL_FLAG_NON_PAGED, …)`（**必須 NonPaged**）

### A2. SDIO bus 抽象 `cyhal_sdio_*`（Zephyr 範本：`mtb-hal-zephyr/.../cyhal_sdio.c`）
- `cyhal_sdio_init / free`
- `cyhal_sdio_send_cmd`（CMD52 direct IO）→ `SDBUS_REQUEST_PACKET` direct
- `cyhal_sdio_bulk_transfer`（CMD53 extended，block/byte mode）→ buffer transfer
- `cyhal_sdio_irq_enable / register_irq` → 註冊 sdbus 中斷 callback（或 OOB GPIO）

### A3. 韌體資源 `whd_resource_source_t`（Zephyr 範本：`wifi-host-driver/.../whd_resources.c`）
三支檔：`cyfmac43455-sdmac.bin`(FW) / `.clm_blob`(地區) / `.txt`(NVRAM)。要提供：
```c
typedef struct {
    uint32_t (*whd_resource_size)(whd_driver_t, whd_resource_type_t, uint32_t *size_out);
    uint32_t (*whd_get_resource_block)(whd_driver_t, whd_resource_type_t, uint32_t blockno,
                                       const uint8_t **data, uint32_t *size_out);  // 分 block 跟你要
} whd_resource_source_t;
```

### A4. 封包/緩衝 callback（與 NetAdapterCx 交接）
- `whd_buffer_funcs_t`：`whd_host_buffer_get / whd_buffer_free / get_current_piece_data_pointer / get_current_piece_size / whd_buffer_add_remove_at_front`
  → **自刻一個 pool 的 `whd_buffer_t`**（含 `data[2048]` + current_ptr），**不要把 NET_PACKET 直接轉型**
  （WHD 會 `add_remove_at_front` 插/拔 SDPCM/BDC header，NET_PACKET 記憶體佈局太嚴）。
- `whd_netif_funcs_t`：`.whd_network_process_ethernet_data = my_rx_handoff`（WHD 收到 802.3 frame 呼叫你）。

---

## B. WHD top-level 使用流程（init → join）
```c
whd_init_config_t   cfg = { .thread_stack_size=8192, .thread_priority=10, .country=WHD_COUNTRY_WORLD_WIDE };
whd_resource_source_t res = { my_res_size, my_res_block };
whd_buffer_funcs_t  buf = { ... };
whd_netif_funcs_t   net = { .whd_network_process_ethernet_data = my_rx_handoff };
whd_driver_t drv; whd_interface_t ifp; whd_mac_t mac;

cyhal_sdio_init(&sdio, ...);                 // 準備跟 sdbus 講話
whd_init(&drv, &cfg, &res, &buf, &net);      // 建 WHD 內部 thread/queue
whd_bus_sdio_attach(drv, &sdio);             // 綁 SDIO
whd_wifi_on(drv, &ifp);                      // ★ 載韌體（耗時數秒，CMD53 把 .bin/.nvram 寫進晶片）
whd_wifi_get_mac_address(ifp, &mac);         // → 回報給 NetAdapterCx
whd_ssid_t ssid = {6,"MyWiFi"};
whd_wifi_join(ifp, &ssid, WHD_SECURITY_WPA2_AES_PSK, (uint8_t*)"pass", 8);  // 阻塞到 4-way handshake
// 成功 → NetAdapterSetLinkState(Up)
```
Rx：WHD thread 讀 SDIO → 剝 SDPCM → 呼叫 `my_rx_handoff(ifp, buffer)` → memcpy 進 NetAdapterCx Rx ring → free buffer。
Tx：`EvtTxQueueAdvance` → `whd_host_buffer_get` → memcpy → `whd_network_send_ethernet_data(ifp, buf)`（WHD 自插 SDPCM、送完自動 free）。

---

## C. SDIO function driver bring-up → 讀 Chip ID 0x4345（第一里程碑）

- **INF**：`SD\VID_02D0&PID_xxxx`（Broadcom VID=0x02D0；PID 需實機/查，常見 0x4345/0xA9BF）；Class=`Net`
  `{4d36e972-e325-11ce-bfc1-08002be10318}`。你的 driver 是 **FDO**，`sdbus.sys` 是 bus。
- **function 列舉**：F0=CCCR（sdbus 掌控）、**F1=Backplane/WLAN 控制（你綁這個）**、F2=WLAN data、F3=BT(另列)。
  多數 Broadcom 驅動掛 F1，透過 F1 讀寫。（sdbus 對 F1/F2 的呈現**需實機驗證**。）
- **取介面**：`WdfFdoQueryForInterface(dev, &GUID_SDBUS_INTERFACE_STANDARD, …)` → 拿 `SubmitRequest`（**PASSIVE_LEVEL 同步**，免手刻 IRP）、`InitializeInterface`(註冊中斷 callback)、`GetProperty/SetProperty`。
- **⚠️ sdbus 多半已做完 CCCR(F0) 初始化**（enable I/O、bus width、INT）；**直接 RAW CMD52 寫 F0 可能被擋或 BSOD**。block size 建議用 `SetProperty(SDBUS_PROPERTY_BLOCK_LENGTH)`。
- **讀 Chip ID（backplane window 機制）**：
```c
// 1) 設 backplane window 到 0x18000000（寫 F1 的 0x1000A/B/C = 高位址）
SdioCmd52(1,0x1000A,&(UINT8){0x00},TRUE); SdioCmd52(1,0x1000B,&(UINT8){0x00},TRUE); SdioCmd52(1,0x1000C,&(UINT8){0x18},TRUE);
// 2) CMD53 讀 4 bytes（window base = 0x8000，對應 backplane 0x18000000 的 chipid）
UINT32 raw; SdioCmd53(1, 0x8000, (UINT8*)&raw, 4, FALSE);
// 3) 低 16 bit == 0x4345 → 里程碑達成
if ((raw & 0xFFFF) == 0x4345) { /* 通了 80% */ }
```
> 若讀 0x8000 卡住：晶片剛上電 backplane clock 可能 sleep，需先 CMD52 寫 `0x1000E`(SDIO core ctrl) 強開 ALP/HT clock（**查 WHD `dhdsdio_clkctl`**）。
- **CMD52/53 packet**：`SDBUS_REQUEST_PACKET` 用 `SDRF_READ/WRITE_PORT_UCHAR`(CMD52) / `SDRF_READ/WRITE_PORT_BUFFER`(CMD53)，填 `Port`(=address)、`Data`/`Buffer`+`Length`。

---

## D. NetAdapterCx 網卡端設定

```c
// EvtDriverDeviceAdd
PNET_DEVICE_INIT ndi = NetDeviceInitAllocate(DeviceInit);
NetDeviceInitConfig(ndi);
WdfDeviceCreate(&DeviceInit, &attr, &device);            // 成功後 ndi 自動清
NETADAPTER_INIT* ai = NetAdapterInitAllocate(device);
NET_ADAPTER_DATAPATH_CALLBACKS dp; NET_ADAPTER_DATAPATH_CALLBACKS_INIT(&dp, EvtCreateTxQueue, EvtCreateRxQueue);
NetAdapterInitSetDatapathCallbacks(ai, &dp);
NetAdapterCreate(ai, &attr, &adapter);  NetAdapterInitFree(ai);
// capabilities
NetAdapterSetPermanentLinkLayerAddress(adapter,&la); NetAdapterSetCurrentLinkLayerAddress(adapter,&la); // la=WHD MAC
NET_ADAPTER_TX_CAPABILITIES tx; NET_ADAPTER_TX_CAPABILITIES_INIT_FOR_FRAGMENT_RING(&tx);
NET_ADAPTER_RX_CAPABILITIES rx; NET_ADAPTER_RX_CAPABILITIES_INIT_SYSTEM_MANAGED(&rx, 1500+14); // OS 配 Rx buffer，你只 memcpy
NetAdapterSetDataPathCapabilities(adapter,&tx,&rx);
NetAdapterStart(adapter);
```
- Tx/Rx queue：`EvtCreateTxQueue` 裡 `NET_PACKET_QUEUE_CONFIG_INIT(&c, EvtTxAdvance, EvtTxCancel, EvtTxSetNotif)` → `NetTxQueueCreate`。
  `EvtTxAdvance` 走訪 `NetTxQueueGetRingCollection` → `NetRingCollectionGetPacketRing/FragmentRing` → `NetRingGetPacketAtIndex/FragmentAtIndex` → `NetExtensionGetFragmentVirtualAddress` 拿 buffer → 丟 WHD。
- **Link state**：初始報 `NET_ADAPTER_LINK_STATE_INIT_DISCONNECTED`；`whd_wifi_join` 成功後在 WorkItem 報 `Connected`。

---

## E. 韌體嵌入 + NVRAM 預處理

- **嵌入方式：用 PE Resource（把 .bin 轉 C array `#include "fw.h"` 或 .rc）**，**別用 ZwReadFile**——
  ARM64 早期開機檔案系統可能還沒掛載；PE resource 跟著 .sys 走、指標直接可用。代價：.sys 大 ~500KB、換韌體要重編。
- `whd_get_resource_block`：算 `offset = blockno * BLOCK`，回 `*data=&arr[offset]`、`*size_out=min(BLOCK, total-offset)`。
  **alignment**：若 WHD 直接拿指標 DMA，資料要 NonPaged + cache-aligned；若 WHD 內部有 bounce buffer 則唯讀指標 OK（需查）。
- **NVRAM `.txt` 在外部先預處理**（Python 腳本），別在 kernel 解析字串：
  ① 刪空行 + `#` 註解行 ② 每行 `\n`→`\0` ③ 檔尾補第二個 `\0`（`\0\0` 結尾）④ 轉 C byte array。

---

## F. Build 整合（WHD 編進 /kernel WDK driver）

- **直接把 WHD 的 .c 逐一加進 driver build 一起 cl /kernel**；**不要編成 user-mode static .lib**（會帶 MSVCRT/浮點/calling-convention → 海量 LNK2019 或 runtime BugCheck）。
- **CRT stub**：`<stdint.h>/<stdbool.h>` WDK 有；`memcpy/memset/memcmp` → ntddk 自動映射 `Rtl*`；`malloc/free` → 你的 `cy_rtos_malloc` 包 `ExAllocatePool2`；**`snprintf` 要自刻** stub 導到 `RtlStringCbVPrintfA`（注意回傳值：Rtl 回 NTSTATUS，要轉成「寫入字元數」）。
- **config macro**（加在 Preprocessor，**確切名稱需查你的 WHD 版本 CMakeLists**）：`WHD_BUS_SDIO`、`WLAN_CHIP_43455`/`CYW43455`、`WHD_CUSTOM_HAL`/`WHD_USE_CUSTOM_OS_LAYER`（叫 WHD 別 include FreeRTOS/ThreadX）、`WHD_NO_ASSERT`（或把 assert 導到 DbgPrint，別 BSOD）。
- **編譯地雷**：
  - `__attribute__((packed))` → 改 WHD 的 `whd_compiler.h`：`WHD_PACK_START=__pragma(pack(push,1))` / `WHD_PACK_END=__pragma(pack(pop))`；硬寫的要全域替換。
  - 浮點（`C2812`）→ 改整數運算（WiFi 很少用浮點，多在統計）。
  - VLA（`int a[n]`）MSVC 不支援 → 改 malloc 或固定 MAX 陣列。

---

## G. 連線流程 + 憑證 + 斷線

- **SSID/password 放 Registry**：`HKLM\System\CurrentControlSet\Services\<driver>\Parameters`；
  `WdfDriverOpenParametersRegistryKey` → `WdfRegistryQueryString(key, L"TargetSSID"/L"TargetPassword", …)`。
  **地雷**：Registry 是 UTF-16，**WHD 要 ASCII/UTF-8** → 用 `RtlUnicodeStringToAnsiString` 轉，否則 SSID 變 `S\0S\0I\0D\0` 連不上。
- **直接 join**（不必先 scan）：`whd_wifi_join(ifp, &ssid, sec, pw, pwlen)`；`whd_security_t` 用 `WHD_SECURITY_WPA2_AES_PSK`
  （除錯先用 `WHD_SECURITY_OPEN` 驗 Tx/Rx 通，再上 WPA2 驗握手；WPA3 韌體太舊會回錯）。
- **別在 PnP callback 死等連線**：`EvtDeviceD0Entry` 裡開 `WdfWorkItem` 跑 join。
- **斷線**：`whd_management_set_event_handler` 監 `WLC_E_LINK`/`WLC_E_DEAUTH` → 報 `NetAdapterSetLinkState(Down)`；
  **不要在 callback 直接重連**（可能 DISPATCH_LEVEL）→ 排 WorkItem，`KeDelayExecutionThread` 退避後重 join 直到連上。

---

## 仍待實機/查 source 的點（誠實標記）
- CYW43455 在 Pi5 掛哪個 SDIO host、能否被 Windows inbox 認出（**最底層先決**）。
- SDIO PID 確切值、F1/F2 在 sdbus 的呈現、SD host 是否自動 1.8V+HS。
- backplane clock 喚醒（`0x1000E` ALP/HT）的確切序列。
- WHD 各 callback / config macro 的**確切名稱與簽章**（依 WHD 版本，對照 Zephyr port + WHD source）。
- 韌體/NVRAM 對應 Pi5 的確切檔（`cyfmac43455-sdmac.*` + Pi 專屬 nvram）。
