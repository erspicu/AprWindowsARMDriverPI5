# 專案守則 (Project Charter)

> 本檔只放**最大守則**。細節見 `MD/Note/`（know-how）、`MD/History/`（開發歷程）、
> `MD/Note/RPi5-Porting-Status.md`（進度真相）。

## 溝通語言
- **一律使用繁體中文與使用者對談。** 技術名詞（API、framework、暫存器、compatible 字串等）可保留英文。

## 專案目標
將 **Raspberry Pi 5**（BCM2712 SoC + RP1 I/O 南橋）的 **Linux 驅動程式**移植到 **Windows on ARM (ARM64)** 平台。

## 移植原則
1. Linux 與 Windows 驅動模型不同，本質是「**重寫**」而非重新編譯。
2. 可沿用：暫存器定義、初始化序列、通訊協定、演算法；必須重寫：OS 介接層（記憶體、中斷、DMA、電源、bus 列舉）。
3. 硬體描述需由 **Device Tree (DT) → ACPI (ASL)**。
4. Windows 框架對應：KMDF/UMDF、NDIS（網路）、SerCx2（UART）、SpbCx（I2C/SPI）、GpioClx（GPIO）、WDDM（顯示/GPU）、AVStream（相機）、PortCls（音訊）。
5. **PCIe 是命脈**：RP1 所有 I/O 掛在 BCM2712 PCIe 底下，需先打通 PCIe 列舉。

## 重要路徑
- `windows_sources/` — Windows 端驅動**原始碼**，依裝置類別分子目錄。
- `windows_driver/` — build 產出（.sys/.cat）+ 手寫 INF。
- `sources/` — Raspberry Pi Linux 核心原始碼（移植參考，未納版控；blobless+sparse，cone 外存取會觸發網路抓取）。
- `MD/Note/` — 分析筆記（檔名 `年月日-時間-主題.md`）；`MD/History/` — 開發歷程。
- `temp/` — **暫存/下載一律放這**，勿散落根目錄。

## 關鍵規則
- **建置**：x64 主機**交叉編譯**至 ARM64（WDK 10.0.26100 + VS2022 MSVC v143 ARM64）。各驅動用 `windows_sources/<類別>/<專案>/build.ps1`（**直接呼叫 cl/link**，winget WDK 無 MSBuild 整合）。include 順序 `wdf\kmdf\1.33`→`km\crt`→`km`→`shared`；KMDF entry `FxDriverEntry`、link `wdfdriverentry.lib`+`wdfldr.lib`。
- **進度真相唯一來源 = `MD/Note/RPi5-Porting-Status.md`**：每完成階段性產出（build 成功/sim 過/ASL 編譯…）就**立即更新**對應列；同時新增 `MD/Note/年月日-時間-主題.md` 記該階段 know-how。硬體全清單 = `MD/Note/RPi5-Driver-Porting-Inventory.md`。
- **狀態階梯**：⬜尚待 → 🟡部分 → 🟢骨架 → 🔵邏輯完整（狀態機全移植＋x64 sim 過）→ ✅完整（實機驗證／純軟體交付物）｜ ➖免驅動。
- 遇技術/規格問題可用 `tools/knowledgebase/gemini_query.py`（金鑰 `C:\key\config.json`，不在 repo 內）諮詢 Gemini。

## 雙機交接模型
- **x64 端（本機）**：cross-compile、補邏輯、寫 x64 sim harness 驗序列、寫 ACPI/INF。硬體驅動推到 **🔵**；純軟體交付物（ACPI/INF）達 **✅**。
- **Pi5 端（實機）**：Windows-on-ARM 載入、KDNET 雙機 WinDbg、校時序/中斷/DMA → 把 **🔵 推到 ✅**。

## Pi5 實機（Linux）SSH — 硬體真相校正
- 有一台真 Pi5（BCM2712，跑 **Debian 13 aarch64**，非 Windows）：`ssh -i ~/.ssh/aprvisual_pi -o BatchMode=yes pi@192.168.0.66 '<cmd>'`（git-bash；IP 非固定，重開機可能變；`sudo -n` 免密碼）。
- **用途＝硬體真相**（唯讀：lspci/iomem/device-tree/clk/devmem/ftrace）；**不能跑/驗證 Windows .sys**。
- **此為使用者 benchmark 機，全程唯讀**：勿 devmem 寫入、勿讀未上電周邊（可能 bus-error）。
- 規格缺漏判別 **A/B 兩類**（重要）：A 類=runtime 才解析（時脈/位址/GIC/硬體能力）→ 量實機；B 類=源碼有寫但移植漏抄 → 重讀源碼補。詳見 `MD/Note/20260621-0720-pi5-linux-hardware-facts.md`。

## 開發歷程
各階段自動移植 loop 的完成紀錄與已解除 cron job → `MD/History/20260621-development-loop-history.md`。
