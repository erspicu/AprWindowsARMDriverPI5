# 第 09 章：ARM64 記憶體模型、MMIO barrier、驅動簽章

> 目標：認識 ARM64 平台特有、會咬人的兩件事——**弱記憶體序（要 barrier）** 與 **驅動簽章**。
> 這兩件在 x86 上不太需要操心，到了 Windows-on-ARM 卻是必修。

## 9.1 弱記憶體序：為什麼 MMIO 要 barrier

ARM64 是**弱記憶體序（weakly-ordered）**：CPU/編譯器可能**重排**讀寫順序。
對一般記憶體沒差，但對 **MMIO（暫存器）** 是災難——你以為「先設好 A、再按下 B 觸發」，
硬體實際看到的順序可能相反。

```
你寫的：    WRITE(CONFIG, x);   // 設定
            WRITE(START, 1);    // 觸發
弱序下可能： START 先到、CONFIG 後到 → 硬體用舊設定就啟動了 → 行為錯亂
```

**解法**：用有序的 MMIO 存取 API + 必要的 barrier。

| Linux | Windows (kernel) | 語意 |
|-------|------------------|------|
| `readl()/writel()` | `READ_REGISTER_ULONG()/WRITE_REGISTER_ULONG()` | 有序的 volatile MMIO 存取（含必要 barrier） |
| `dma_wmb()/dma_rmb()` | `KeMemoryBarrier()` 等 | 顯式記憶體屏障 |
| `readl_relaxed()` | `READ_REGISTER_*`（無額外屏障語意需自理） | 放寬序（少用） |

> **本專案規則**：HAL 一律用 `READ_REGISTER_ULONG`/`WRITE_REGISTER_ULONG`（對應 Linux `readl/writel`），
> 不要用裸指標解參考（`*(volatile ULONG*)`）——後者編譯器可能優化掉或重排。

## 9.2 為什麼 `READ/WRITE_REGISTER_*` 而不是裸指標？

```c
// ❌ 危險：編譯器/CPU 可能重排、合併、甚至省略
*(volatile ULONG*)(Base + START) = 1;

// ✅ 正確：Windows 保證對 MMIO 的有序存取
WRITE_REGISTER_ULONG((volatile ULONG*)(Base + START), 1);
```
在 x86 上兩者常常剛好都對（強記憶體序掩蓋問題）；在 ARM64 上差別會真的爆出來。
**所以本專案的 regio shim 真機端一律用 `READ/WRITE_REGISTER_*`**（見 [第 06 章](06-porting-methodology.md)）。

## 9.3 對齊與存取寬度

- MMIO 存取要**對齊且用正確寬度**：8/16/32-bit 暫存器分別用 `READ_REGISTER_UCHAR/USHORT/ULONG`。
  例：SDHCI 的 `BLOCK_SIZE` 是 16-bit、`SOFTWARE_RESET` 是 8-bit，混用寬度會出錯。
- 64-bit 位址（>4GB，如 BCM2712 的 `0x10_0000_0000` 區）在 ACPI 要用 **QWordMemory**（[第 04 章](04-device-tree-to-acpi.md)）。

## 9.4 驅動簽章（Windows-on-ARM 特別嚴）

Windows ARM64 對驅動載入要求簽章，**不能像 x86 隨便載**：

| 階段 | 做法 |
|------|------|
| **開發 / 測試** | 開 **test-signing** 模式（`bcdedit /set testsigning on`）+ 自簽憑證，可載入測試驅動 |
| **正式發佈** | 需 **EV 憑證** + 送微軟 **attestation signing / WHQL**（HLK 測試） |

開發流程通常：
1. `bcdedit /set testsigning on`（在 Pi5/Win11-ARM 上）+ 重開機。
2. 用 `MakeCert`/`signtool` 自簽 `.sys` + `.cat`。
3. `pnputil /add-driver x.inf /install` 安裝。
4. KDNET 雙機接 WinDbg 偵錯。

> **本專案現況**：x64 端產出未簽章的 `.sys`（邏輯+模擬驗證）；簽章 + 載入屬 Pi5 實機階段（🔵→✅）。

## 9.5 INF / catalog 注意

- INF 要宣告 `PnpLockdown = 1`、正確的 `Class`/`ClassGuid`、KMDF 版本（`KmdfLibraryVersion=1.33`）。
- `.cat`（catalog）對 INF 內所有檔案做雜湊；簽 `.cat` 等於背書整包。
- `infverif` 先在 x64 端把 INF 語法/規則驗過（見 [第 10 章](10-build-your-first-driver.md)）。

## 9.6 其他 ARM64 注意

- **DMA 與 cache coherency**：ARM64 上 device DMA 與 CPU cache 的一致性要靠正確的 buffer 屬性
  （non-cached / 適當 flush），這部分屬實機階段細調。
- **IOMMU**：BCM2712 有 IOMMU，DMA 位址轉換要納入考量（高難度）。

## 本章重點

- ARM64 **弱記憶體序** → MMIO 一律用 `READ/WRITE_REGISTER_*`（別用裸 volatile 指標）。
- 用**正確寬度**存取（8/16/32-bit）；>4GB 位址用 QWordMemory。
- Windows-ARM64 **簽章嚴**：開發用 test-signing + 自簽；正式要 EV + WHQL。
- 簽章/載入/DMA coherency 屬 Pi5 實機階段。

➡️ 下一章：[建置工具鏈與你的第一個驅動](10-build-your-first-driver.md)
