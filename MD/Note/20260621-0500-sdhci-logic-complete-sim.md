# SDHCI 命令引擎 🔵 邏輯完整 + x64 模擬 harness pattern

> 日期：2026-06-21（loop 第 4 輪）｜ 首個達 🔵 的硬體驅動

## 產出
- `windows_sources/storage/rp1-sdhci/`：
  - `sdhci_hw.h/.c` — 標準 SDHCI 命令引擎 HAL（reset/clock/power/sendcmd/readresp/intstatus/ack/intenable/cardpresent/setblock），純暫存器邏輯、無 OS 型別。
  - `sdhci_regio.h` — 暫存器存取 shim：kernel 用 `READ/WRITE_REGISTER_*`，`#define SDHCI_SIM` 用 mock。
  - `sim/sdhci_sim.c` — x64 使用者態 harness：mock 暫存器 + 裝置行為，跑命令序列斷言。**18/18 PASS**。
  - `miniport.c` — SdPort callback 接上引擎（IssueRequest→SendCommand、Interrupt→GetIntStatus+Ack、GetResponse→ReadResponse、Initialize→reset/power/clock/inten、GetCardDetectState→CardPresent）。
- `windows_driver/storage/rp1sd.sys` 重建（9.7 KB，ARM64，import sdport.sys）。

## 🔵「邏輯完整 + 模擬驗證」pattern（可複用於 I2C/Ethernet…）
1. **抽 HAL**：把暫存器狀態機寫成不依賴 OS 型別的函式（參數 `void* Base` + 純量），回傳 0/-1，poll 加 bounded retry（避免 sim 無限迴圈、實機亦較安全）。
2. **reg I/O shim**（`*_regio.h`）：`#ifdef <X>_SIM` → mock `SimRd/SimWr`；否則 `READ/WRITE_REGISTER_*`。
3. **sim harness**（`sim/*.c`，`/D<X>_SIM`）：mock 暫存器陣列 + 在 `SimWr` 內模型化裝置 side-effect（reset 自清、clock stable、cmd→int complete、W1C…），main 跑序列 + `check()` 斷言。
4. **接 OS 層**：framework callback（SdPort/SpbCx/NDIS）翻譯成 HAL 呼叫。
5. **build 兩邊**：driver build.ps1 非遞迴 glob（`sim/` 自動排除）；sim 用 host x64 cl（user-mode include：VC + ucrt）。

## 模擬指令（x64）
`cl /DSDHCI_SIM /I<proj> /I<vc>\include /I<ucrt> sim\sdhci_sim.c sdhci_hw.c /link /LIBPATH:<vc>\lib\x64 /LIBPATH:<ucrt>\lib\x64 /LIBPATH:<um>\lib\x64`

## 餘下（需實機）
SDHCI DMA/ADMA2 資料搬移、PIO buffer、tuning（SDR104/HS200）、voltage switch（1.8V）、實際時脈分頻校正。

## 下一個 🔵 目標
I2C（DesignWare）傳輸狀態機 或 Ethernet（GEM）TX/RX ring，套同一 sim pattern。
