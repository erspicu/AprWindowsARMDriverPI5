# tools — 輔助工具

## knowledgebase/ — Gemini 諮詢腳本

遇技術/規格問題時，用 `gemini_query.py` 諮詢 Gemini 作參考。

- `gemini_query.py` — 查詢腳本。
- `config.json.example` — 設定範本。**實際金鑰**放在 `C:\key\config.json`（含 `api_key`、`model`），
  **不在本 repo 內**；腳本啟動時讀取該外部路徑。
- `models_list.txt` / `readme.txt` — 模型清單與說明。
- `message/` — 每次查詢的問答紀錄（`年月日_時分秒.md`，markdown 格式，含 WiFi/藍牙等技術諮詢全文）。

```powershell
python tools\knowledgebase\gemini_query.py "你的問題"
```
