# MD — 文件

移植過程的文件，分三類子目錄：

| 子目錄 | 用途 |
|--------|------|
| [`Note/`](Note/) | 各階段 **know-how 筆記**（檔名 `年月日-時間-主題.md`）+ 進度/清單核心文件。 |
| [`Skill/`](Skill/) | **操作型 how-to**（建置工具鏈、Pi5 SSH 硬體萃取…），CLAUDE.md 細節引導至此。 |
| [`teachbook/`](teachbook/) | **教學書 × Pi5 硬體規格書**：Windows 驅動移植教學（基礎觀念 + 逐一硬體規格與移植，附 code sample）。持續撰寫中。 |
| [`History/`](History/) | **開發歷程**（已完成/解除的自動移植 loop 紀錄）。 |

## Skill（操作 how-to）

| 檔案 | 內容 |
|------|------|
| [`Skill/build-toolchain.md`](Skill/build-toolchain.md) | x64→ARM64 交叉編譯、cl/link 配方、x64 sim、ASL/INF。 |
| [`Skill/pi5-ssh-hardware-facts.md`](Skill/pi5-ssh-hardware-facts.md) | Pi5 SSH 連線法、唯讀安全規則、規格缺漏 A/B 判別、已萃取硬體事實。 |

## 核心文件（單一真相來源）

| 檔案 | 內容 |
|------|------|
| [`Note/RPi5-Porting-Status.md`](Note/RPi5-Porting-Status.md) | **全 Pi5 驅動移植狀態清單**（狀態階梯 ⬜🟡🟢🔵✅➖、雙機交接盤點、各驅動產出路徑/餘下項目）。每完成階段性產出即更新。|
| [`Note/RPi5-Driver-Porting-Inventory.md`](Note/RPi5-Driver-Porting-Inventory.md) | 硬體**全裝置清單**與 Windows 驅動移植對照表。|
| [`Note/RPi5-Porting-Difficulty-List.md`](Note/RPi5-Porting-Difficulty-List.md) | **移植難易清單**：好移植 vs 難移植（如 WiFi）分類 + 逐一原因。|

## 重點 know-how 筆記

| 檔案 | 主題 |
|------|------|
| [`Note/20260621-0720-pi5-linux-hardware-facts.md`](Note/20260621-0720-pi5-linux-hardware-facts.md) | Pi5 實機（Linux）SSH 唯讀萃取的**硬體事實**（BAR/偏移/IRQ/時脈）+ 「規格缺漏 A/B 兩類」判別原則。|
| [`Note/20260621-0820-source-vs-port-bclass-gaps.md`](Note/20260621-0820-source-vs-port-bclass-gaps.md) | 「Linux 源碼 vs 移植」逐項比對，14 個 HAL 的 **B 類缺漏**補正紀錄。|
| `Note/20260621-*-*.md` | 各階段（ACPI/SDHCI/PCIe/bespoke KMDF/sim 等）的逐步紀錄。|

> 筆記檔名慣例：`年月日-時間-主題.md`（例 `20260621-0720-pi5-linux-hardware-facts.md`）。
