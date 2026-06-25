# 專案守則 (Project Charter)

> 本檔只放**基本守則/規範/定義**。細節依分類引導：
> - 操作型 how-to（建置、Pi5 SSH 萃取…）→ `MD/Skill/`
> - 各階段 know-how 筆記 → `MD/Note/`（檔名 `年月日-時間-主題.md`）
> - 開發歷程（已完成 loop）→ `MD/History/`
> - **進度真相** → `MD/Note/RPi5-Porting-Status.md`；硬體全清單 → `MD/Note/RPi5-Driver-Porting-Inventory.md`

## 溝通語言
**一律使用繁體中文與使用者對談。** 技術名詞（API、framework、暫存器、compatible 字串等）可保留英文。

## 專案目標
將 **Raspberry Pi 5**（BCM2712 SoC + RP1 I/O 南橋）的 **Linux 驅動程式**移植到 **Windows on ARM (ARM64)** 平台。

## 移植原則
1. Linux 與 Windows 驅動模型不同，本質是「**重寫**」而非重新編譯。
2. 可沿用：暫存器定義、初始化序列、通訊協定、演算法；必須重寫：OS 介接層（記憶體、中斷、DMA、電源、bus 列舉）。
3. 硬體描述需由 **Device Tree (DT) → ACPI (ASL)**。
4. Windows 框架對應：KMDF/UMDF、NDIS（網路）、SerCx2（UART）、SpbCx（I2C/SPI）、GpioClx（GPIO）、WDDM（顯示/GPU）、AVStream（相機）、PortCls（音訊）。
5. **PCIe 是命脈**：RP1 所有 I/O 掛在 BCM2712 PCIe 底下，需先打通 PCIe 列舉。

## 路徑分類（什麼放哪）
- `windows_sources/` — Windows 端驅動**原始碼**，依裝置類別分子目錄。
- `windows_driver/` — build 產出（.sys/.cat）+ 手寫 INF。
- `sources/` — Raspberry Pi Linux 核心原始碼 + Infineon WHD（移植參考，**未納版控**，~1.5GB）。**重建法（換機/Win11 ARM 端必看）→ `MD/Skill/sources-rebuild.md`**。
- `MD/` — 文件：`Skill/`(how-to)、`Note/`(know-how)、`History/`(歷程)。
- `temp/` — **暫存/下載一律放這**，勿散落根目錄。

## 進度規範
- **單一進度真相 = `MD/Note/RPi5-Porting-Status.md`**：每完成階段性產出（build/sim/ASL…）就**立即更新**對應列；同時新增 `MD/Note/年月日-時間-主題.md` 記該階段 know-how。
- **狀態階梯定義**：⬜尚待 → 🟡部分 → 🟢骨架 → 🔵邏輯完整（狀態機全移植＋x64 sim 過）→ ✅完整（實機驗證／純軟體交付物）｜ ➖免驅動。

## 雙機交接模型
- **x64 端（本機）**：cross-compile、補邏輯、寫 x64 sim 驗序列、寫 ACPI/INF。硬體驅動推到 **🔵**；純軟體交付物（ACPI/INF）達 **✅**。
- **Pi5 端（實機）**：Windows-on-ARM 載入、KDNET 雙機 WinDbg、校時序/中斷/DMA → 把 **🔵 推到 ✅**。

## 操作引導（細節在 Skill）
- **建置**（交叉編譯、cl/link 配方、sim、ASL/INF）→ `MD/Skill/build-toolchain.md`。
- **Pi5 SSH 硬體真相萃取**（連線法、唯讀安全規則、規格缺漏 A/B 判別）→ `MD/Skill/pi5-ssh-hardware-facts.md`。
- 技術/規格諮詢：`tools/knowledgebase/gemini_query.py`（金鑰 `C:\key\config.json`，不在 repo 內）。
