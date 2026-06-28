# D2：Bluetooth（BCM43438，UART HCI）

> 教「**走標準協定（HCI over UART）的裝置怎麼移植**」——重點在協定 framing 與韌體載入，不只暫存器。
>
> ⚠️ **路線更新（2026-06-25）**：原規劃自寫 `bthx.h` transport driver，但查證後 **`bthx.h` 不在現代公開 WDK**（Win8 IHV-only）。
> 已改走 **inbox `BthUart.sys`**（UART 下掛 ACPI + INF `Needs=BthUart.NT`），我們只做 BCM `.hcd` 韌體載入 + baud 切換的薄處理。
> 本章下方若提到 bthx transport，請以此更正為準；實作見 `windows_sources/bluetooth/bcm43438`。

| | |
|---|---|
| **裝置** | BCM43438 Bluetooth（與 WiFi 同模組，BT 走 UART） |
| **Linux 源碼** | `drivers/bluetooth/hci_bcm.c`（+ `btbcm.c`） |
| **Windows 框架** | bthport（序列 HCI / BthMini） |
| **本專案** | `windows_sources/bluetooth/bcm43438/` ｜ INF `btbcm.inf` ｜ 狀態 🔵（sim 12/12，缺 bthx.h SDK） |

## 1. 和前面裝置不一樣的地方

前面多數是「設暫存器」；BT 是「**透過 UART 跟一個獨立的 BT 控制器講 HCI 協定**」。所以重點不在 MMIO，而在：
- **H4 framing**：每個封包前綴一個 type byte（`0x01`=command、`0x02`=ACL、`0x04`=event）。
- **BCM 廠商初始化**：開機要送一串 vendor command + **載入韌體 patch（.hcd）**，BT 才會動。
- **波特率切換**：初始低速 → 載完韌體切高速。

## 2. H4 framing（code sample）

```c
// 把一個 HCI command 包成 H4：[0x01][opcode_lo][opcode_hi][plen][params...]
int BtBcmBuildCommand(UCHAR *out, USHORT opcode, const UCHAR *params, UCHAR plen) {
    out[0] = 0x01;                       // HCI command packet
    out[1] = opcode & 0xFF;
    out[2] = opcode >> 8;
    out[3] = plen;
    for (int i = 0; i < plen; i++) out[4+i] = params[i];
    return 4 + plen;
}
```

## 3. BCM 初始化序列（從 hci_bcm 抄）

```
HCI_RESET
→ 讀晶片版本（決定 .hcd 韌體檔）
→ 進 download mode → 一塊塊送韌體 patch
→ 重啟、切波特率
→ 設 BD_ADDR、UART flow control
→ 完成，交給 BT stack
```
> 教學點：**韌體載入是很多無線/多媒體裝置的隱形步驟**。沒載韌體，裝置就是塊磚。
> .hcd 韌體檔屬廠商 binary，要另外取得並放對位置。

## 4. Windows 框架現況

- Windows 的序列 BT 走 **bthport / BthMini**，需要 `bthx.h` 等 SDK 標頭（本專案環境暫缺）。
- 本專案完成 H4 framing + BCM init table 的 HAL + x64 sim（12/12）；接 bthport 屬待補 SDK + 實機。

➡️ 回 [目錄](README.md)
