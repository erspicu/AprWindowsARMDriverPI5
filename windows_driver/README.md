# windows_driver — 建置交付物

各驅動的**安裝/部署檔**，依裝置類別分子目錄（對應 `windows_sources/` 的類別）。

## 納入版控的內容（方便直接下載編譯好的版本）

- **`*.inf`** — 各驅動的安裝資訊檔（PnP ID 對齊 ACPI `_HID`；KMDF 版本宣告；均通過 `infverif`）。手寫交付物。
- **`*.sys`** — **已納版控**：ARM64 cross-compile 產出的 25 個驅動（adc/audio/bt/camera/clk/display/dma/
  gpio×2/gpu/i2c×2/mailbox/net/pcie-rp1/pio/pwm/rng/rtc/spi×2/storage/watchdog/wifi…），方便他人直接取用。
  > ⚠️ 目前為 **test-signing**（開發用）；正式部署需簽章（見 `MD/Note/20260626-0130-...packaging-sop.md`）。

## 未納版控（build 中間產物）

- `windows_sources/<專案>/build/` 內的中間 obj/pdb、`*.cat`（需簽章）→ `.gitignore` 排除。

## 重建方式

```powershell
cd ..\windows_sources\<類別>\<專案>
.\build.ps1            # 產出 .sys 回此目錄對應子資料夾
```

INF 與 ACPI `_HID` 對照、各驅動服務名等，見根目錄 [`../README.md`](../README.md) 與
[`../MD/Note/RPi5-Porting-Status.md`](../MD/Note/RPi5-Porting-Status.md)。
