# C6：MIPI CSI 相機前端 ×2（RP1 CFE，AVStream）

| | |
|---|---|
| **裝置** | RP1 CSI-2 相機前端（CFE，×2） |
| **Linux 源碼** | `drivers/media/platform/raspberrypi/rp1_cfe/cfe.c` |
| **Windows 框架** | **AVStream**（KS filter / capture pin） |
| **本專案** | AVStream minidriver 註冊骨架（`rp1cfe.sys`，C++/ks.sys），🟡 |

## 1. 相機流水線

```
sensor(I2C 控制) → MIPI CSI-2 → [CFE(本章) 收 RAW] → [ISP/PiSP(C5)] → video frame → AVStream pin
```
- **CFE**：接收 CSI-2 的影像資料、寫進記憶體 buffer。
- sensor 本身（如 IMX 系列）透過 [I2C（B9）](B09-rp1-designware-i2c.md) 控制（設曝光/增益/模式）。

## 2. Windows 框架（AVStream）

- AVStream 用 **KS（Kernel Streaming）** 模型：filter / pin / KSPROPERTY。
- 要實作 capture KSFILTERFACTORY：CSI-2 sensor → ISP → video pin，加 DMA/buffer 佇列。

## 3. 移植踩雷（本專案已遇到）

- `ks.h` 在 C 編譯會出 C2054/C2085 → **要以 C++ 編譯**，`DriverEntry` 包 `extern "C"`。
- 還要補 `typedef int BOOL;` 與 `WINAPI` 定義（kernel 標頭缺）。

## 4. 狀態

AVStream 註冊骨架已建；完整 capture（filter/pin/DMA、sensor 控制、與 ISP 串接）屬 Tier 3 + 實機。

➡️ 回 [目錄](README.md)
