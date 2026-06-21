# MD — 分析筆記與文件

移植過程的設計筆記、進度狀態與各階段 know-how。

## 核心文件（單一真相來源）

| 檔案 | 內容 |
|------|------|
| [`Note/RPi5-Porting-Status.md`](Note/RPi5-Porting-Status.md) | **全 Pi5 驅動移植狀態清單**（狀態階梯 ⬜🟡🟢🔵✅➖、雙機交接盤點、各驅動產出路徑/餘下項目）。每完成階段性產出即更新。|
| [`Note/RPi5-Driver-Porting-Inventory.md`](Note/RPi5-Driver-Porting-Inventory.md) | 硬體**全裝置清單**與 Windows 驅動移植對照表。|

## 重點 know-how 筆記

| 檔案 | 主題 |
|------|------|
| [`Note/20260621-0720-pi5-linux-hardware-facts.md`](Note/20260621-0720-pi5-linux-hardware-facts.md) | Pi5 實機（Linux）SSH 唯讀萃取的**硬體事實**（BAR/偏移/IRQ/時脈）+ 「規格缺漏 A/B 兩類」判別原則。|
| [`Note/20260621-0820-source-vs-port-bclass-gaps.md`](Note/20260621-0820-source-vs-port-bclass-gaps.md) | 「Linux 源碼 vs 移植」逐項比對，14 個 HAL 的 **B 類缺漏**補正紀錄。|
| `Note/20260621-*-*.md` | 各階段（ACPI/SDHCI/PCIe/bespoke KMDF/sim 等）的逐步紀錄。|

> 筆記檔名慣例：`年月日-時間-主題.md`（例 `20260621-0720-pi5-linux-hardware-facts.md`）。
