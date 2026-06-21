# 藍牙移植：bring-up 里程碑 + 電源 + SCO + 待驗證清單

## 1. Bring-up 里程碑（每步明確驗收點）

| M | 動作 | 驗收成功 = |
|---|------|-----------|
| **M1 硬體就緒** | driver 載入、解析 ACPI 取得 UART + GPIO(BT_REG_ON/HOST_WAKE)、`WdfIoTargetOpen` UART | 裝置節點無錯（若故意 hold D0Entry 允許 Code 10）；WPP 顯示 open 成功 |
| **M2 晶片心跳 🚀第一里程碑** | 拉高 BT_REG_ON、等 100ms、UART 送 `HCI_Reset`(`01 03 0C 00`) | UART RX 收到 `04 0E 04 01 03 0C 00`（command complete）= **UART TX/RX 正常 + baud 對 + 晶片活** |
| **M3 韌體 + baud** | Update_Baud_Rate→host 切 baud→分塊寫 .hcd→再 HCI_Reset | 切 baud 後的 HCI_Reset 仍收到 `04 0E…` = 韌體套用 + 高速 UART 穩 |
| **M4 BTHX 疊合** | 完成 init、回應 bthport 的 `IOCTL_BTHX_SET_VERSION`、處理 READ/WRITE_HCI | Windows 出現藍牙圖示；裝置管理員「藍牙」下出現 "Bluetooth Radio"（bthport 認領） |
| **M5 連線資料流** | Windows 藍牙設定掃描 + 配對 | 配對滑鼠/鍵盤能動/能打字（ACL 流動正確、read pump 沒掉 byte） |

> **開發節奏**：先寫最小 driver 只做 M1→M2（開 UART + 拉 GPIO + 送 HCI_Reset + DbgPrint）。看到 `04 0E…` 後，剩下是體力活。

## 2. 電源管理

| 情境 | 做法 |
|------|------|
| **D0Exit → Modern Standby (S0ix)** | **不要拉低 BT_REG_ON**（要保留喚醒能力）；下 vendor command 讓晶片進 sleep/low-power |
| **D0Exit → S4/S5（休眠/關機）** | 拉低 BT_REG_ON 斷電 |
| **D0Entry ← S0ix** | UART 恢復通訊，**不需重載 .hcd**（晶片沒斷電）|
| **D0Entry ← cold boot** | BT_REG_ON 拉高，**必須重走 M3**（重載 .hcd + baud） |
| **BT_HOST_WAKE（OOB GPIO 中斷）** | host 低功耗時晶片收到封包會拉高此腳 → 實作 **arm-for-wake**（`WdfDeviceAssignS0IdleSettings` / `EvtDeviceArmWakeFromS0`）；中斷觸發時 WDF 自動帶回 D0、read pump 恢復 |
| **radio on/off（飛航模式）** | bthport **不發專門 power IOCTL**，靠標準 HCI 控制；你只要忠實傳 HCI 即可 |
| **D0Exit 時 RTS** | 睡前 host RTS 要拉高（叫 BT 別再傳），否則 host 睡著時 BT 灌資料 → UART overrun / 藍屏（**需實機調**） |

## 3. SCO/eSCO（藍牙音訊）策略

- **A2DP（聽音樂/追劇）走 ACL（0x02）**：經 UART，read pump 會處理。UART 夠快（3Mbps）就行。**MVP 可用**。
- **SCO/eSCO（通話/HFP 麥克風）走 SCO（0x03）**：在 ARM64 SoC 上**通常不走 UART**（延遲太高會斷續），而是 BT 晶片用實體 **PCM/I2S** 線直連 SoC audio codec。
  - **MVP 階段：不管 SCO**。
  - 真要做：BCM vendor command（`Write_SCO_PCM_Int_Param` 類）告訴晶片把語音路由到 I2S 腳，不發 UART；若 read pump 真收到 0x03，按長度讀完原封丟給 bthport。

## 4. 待驗證 / 實機釐清清單 ⚠️

| 分類 | 待確認 | 為何重要 |
|------|--------|----------|
| BTHX API | `bthx.h` 的 `BTHX_HCI_READ_WRITE_CONTEXT` 確切定義（Type/Length 欄位）| 回傳封包給 bthport 要填對 metadata |
| BTHX API | `IOCTL_BTHX_SET_VERSION` 要回報哪個版本才能讓 bthport 滿意 | 不對 bthport 不疊上 |
| INF | `Needs`/`Include` 確切寫法（`BthUsb.NT` vs `BthUart.NT` vs 自訂）| 決定 bthport 是否掛到你頭上 |
| BCM 韌體 | .hcd 每筆 chunk payload 上限（通常 **252 bytes**）| 超過會失敗 |
| 休眠 | D0Exit 時 RTS/CTS 行為 | 沒處理好 → overrun/藍屏 |
| MAC | BCM MAC 來源（OTP？ACPI？Registry？vendor `Write_BD_ADDR`）| 你的 driver 要不要負責寫 |
| ACPI | Pi5 BT 接哪個 PL011（debug 還是專用）+ BT_REG_ON/HOST_WAKE 的 GPIO pin 號 | ASL `_CRS` 要填對 |
| 平台 | bthport 在 Windows ARM64 是否完整支援 BTHX UART transport 路徑 | 最底層可行性（**先驗**）|

## 5. 與 WiFi 的關係（同模組）
WiFi（[../wifi/](../wifi/)）與 BT 是 **CYW43455 同一顆 combo**：WiFi 走 SDIO、BT 走 UART，是兩條**完全獨立**的驅動路線。
共通點：都要載廠商韌體 blob、都接在 Pi5 的 RP1 周邊上、都需 ACPI 正確描述資源。
