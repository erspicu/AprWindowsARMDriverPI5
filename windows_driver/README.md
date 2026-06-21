# windows_driver — 建置交付物

各驅動的**安裝/部署檔**，依裝置類別分子目錄（對應 `windows_sources/` 的類別）。

## 納入版控的內容

- **`*.inf`** — 各驅動的安裝資訊檔（PnP ID 對齊 ACPI `_HID`；KMDF 版本宣告；均通過 `infverif`）。
  這些是**手寫交付物**，故納入版控。

## 未納版控（build 產出，可重建）

- `*.sys`（ARM64 kernel 驅動）、`*.cat`（catalog）、`*.pdb` 等二進位 → 由
  `windows_sources/<類別>/<專案>/build.ps1` 交叉編譯產生，已在 `.gitignore` 排除。
- 編譯後的 ACPI `*.aml` 同樣排除（由 `rp1.asl` 重建）。

## 重建方式

```powershell
cd ..\windows_sources\<類別>\<專案>
.\build.ps1            # 產出 .sys 回此目錄對應子資料夾
```

INF 與 ACPI `_HID` 對照、各驅動服務名等，見根目錄 [`../README.md`](../README.md) 與
[`../MD/Note/RPi5-Porting-Status.md`](../MD/Note/RPi5-Porting-Status.md)。
