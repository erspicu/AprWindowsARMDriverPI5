# IOMMU / SMMU 移植筆記

> Pi5（BCM2712）的 IOMMU 在 **Windows on ARM64** 怎麼處理。
> 來源：本專案分析 + Gemini 1 輪諮詢（`tools/knowledgebase/message/`）。建立：2026-06-22

## 一句話結論（最重要的釐清）

> **不需要寫「IOMMU 驅動」。** Windows ARM64 的 SMMU 由 **HAL + DMA 子系統內建支援**（inbox SMMUv1/v2/v3）。
> 「移植 IOMMU」實際上 ＝ **把 BCM2712 的 SMMU 拓樸正確寫進 ACPI IORT table**（在 UEFI/EDK2 裡），
> 讓 Windows HAL 認得並自動建立 DMA 映射。Driver 開發者只用標準 WDF DMA（`WdfCommonBuffer`）即可。

## 為什麼它仍被列為「難」
不是因為要寫 `.sys`，而是 **debug ACPI table + 硬體 SMMU 暫存器狀態極困難**，而且**整個 PCIe/RP1 的 DMA（相機、網路、所有 RP1 周邊）成敗都繫於此**——IORT 寫錯 → SMMU fault → BugCheck。

## 關鍵事實
- BCM2712 SMMU = **ARM MMU-500（SMMUv2）**，Windows ARM64 完全支援。
- RP1 掛在 PCIe 上 → RP1 內所有周邊的 DMA 對 SMMU 而言帶**同一組 PCIe Requester ID**。
- Win11 ARM64 因 HVCI/Kernel DMA Protection，**IOMMU 近乎強制**；IORT 沒寫對可能開機早期就 panic。

## 實際要做的事（在 UEFI，不是 WDK）
1. **UEFI/EDK2**：確保 SMMU clock/power 在 UEFI 階段已開、未被 TrustZone 鎖為 secure-only；把 PCIe AXI 路由進 MMU-500。
2. **寫 ACPI IORT**（ARM DEN0049）：SMMUv2 node（base/IRQ）+ PCI RC node + ID mapping（PCIe RID→SMMU Stream ID）。
3. **DSDT/MCFG**：`Device(PCI0)` 的 `_CCA`（cache coherency）要對（影響 DMA buffer 要不要 cache flush）。

## 最大未知數
**RP1 內部各周邊（USB/Ethernet/相機…）發 DMA 時是共用一個 PCIe BDF/RID 還是各自不同？** → 決定 IORT 的 ID mapping 陣列怎麼寫。**需實機/查 Linux DT 驗證。**

## 詳見
[`01-iort-implementation.md`](01-iort-implementation.md)：IORT 三 node 結構、IORT 寫錯的兩種後果、工程清單、參考。

## 與其他筆記的關係
這是 [`camera/`](../camera/)「PCIe DMA/IOMMU 生死線」、以及所有 RP1 周邊 DMA 的底層先決條件。
