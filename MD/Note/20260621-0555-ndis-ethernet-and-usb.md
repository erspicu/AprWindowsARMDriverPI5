# RP1 Ethernet（NDIS miniport）+ USB inbox know-how

> 日期：2026-06-21 05:55

## 1. RP1 Gigabit Ethernet（Cadence GEM）— NDIS 6.30 miniport
- 產出 `windows_driver/net/rp1gem.sys`（ARM64, import NDIS.SYS）。
- **NDIS contract**：include `ndis.h` 前先 `#define NDIS_MINIPORT_DRIVER 1` / `NDIS630_MINIPORT 1` / `NDIS_SUPPORT_NDIS6 1`。link **`ndis.lib`**，entry `GsDriverEntry`（非 KMDF）。
- `DriverEntry` → 填 `NDIS_MINIPORT_DRIVER_CHARACTERISTICS`（Header: Type/Revision_2/Size、Major=6 Minor=30、13 個 handler）→ `NdisMRegisterMiniportDriver`。
- 13 handler 用 ndis.h 的函式 typedef 前向宣告：`MINIPORT_INITIALIZE/HALT/PAUSE/RESTART/OID_REQUEST/SEND_NET_BUFFER_LISTS/RETURN_NET_BUFFER_LISTS/CANCEL_SEND/CHECK_FOR_HANG/RESET/DEVICE_PNP_EVENT_NOTIFY/SHUTDOWN/CANCEL_OID_REQUEST`。
- `MINIPORT_INITIALIZE`：`NdisAllocateMemoryWithTagPriority` 配 adapter → 填 `NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES`（Revision_1）→ `NdisMSetMiniportAttributes`。
- Send 暫以失敗完成（`NET_BUFFER_LIST_STATUS=FAILURE` + `NdisMSendNetBufferListsComplete`）避免協定堆疊卡住。
- **餘下需實機**：general attributes（MediaType/link 速率/MAC/支援 OID 清單）、GEM TX/RX DMA ring、MDIO PHY（broadcom）、實際 OID 處理。

## 2. USB3（RP1 dwc3）— 免驅動
- dwc3 host 模式 = **xHCI 相容** → Windows **inbox `USBXHCI.sys`**（ACPI `_HID PNP0D10`）。標 ➖。
- 內建 USB-OTG（dwc2）為次要，亦可走 inbox/自訂。

## 3. 下一步（使用者指定：wifi 藍牙）
- **Bluetooth（BCM43438, UART H4/H5 HCI）**：Windows 走 serial HCI transport（ACPI `BCM2EA0` 類 + UART）；可做 KMDF transport 骨架。
- **WiFi（CYW43455, SDIO, brcmfmac）**：**NDIS 802.11（Native WiFi）miniport**，工程量最大（dot11 OID 海量）；先做 NDIS 註冊骨架。
