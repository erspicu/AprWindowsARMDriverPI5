# IOMMU：ACPI IORT 實作細節

> 承 [`README`](README.md)。**這是 UEFI/EDK2 的 ACPI 工作，不是 WDK 驅動。** 暫存器/SID 對應一律標「需查 Linux bcm2712.dtsi / 需實機」。

## 1. Windows ARM64 的 SMMU 運作流程（事實）

```
driver 呼叫 WdfCommonBufferCreate
  → Windows DMA framework (DMAv3) 查 ACPI IORT
  → 找到該 device 的 SMMU + Stream ID(SID)
  → 在 SMMU 建 page table，把 physical addr 映射成 IOVA
  → 回傳 IOVA 給 driver（WdfCommonBufferGetAlignedLogicalAddress）
  → 硬體用 IOVA 發 DMA → SMMU 硬體自動轉回 physical
```
- Windows 對標準 ARM **SMMUv1 / SMMUv2(MMU-400/500) / SMMUv3** 都有 **inbox 支援**；**不能也不需要**寫第三方 IOMMU `.sys`。

## 2. BCM2712 SMMU 拓樸（事實）

- SMMU = **ARM MMU-500 = SMMUv2 架構**（依 `bcm2712.dtsi` + Broadcom 公開資訊）。
- Linux DT 描述：
  - SMMU node：`compatible = "arm,mmu-500"`。
  - PCIe RC：用 `iommu-map` property 定義 `PCIe RID --(offset)--> SMMU Stream ID`。
  - RP1 是 PCIe endpoint → RP1 內所有周邊 DMA 帶同一（組）PCIe RID 進 SMMU。

## 3. ACPI IORT 結構（翻成 Windows 看得懂）

依 **ARM DEN0049（IORT spec）**，IORT 至少三個互連 node：
1. **SMMUv1/v2 Node**：MMU-500 的 base address、interrupts（注意 GIC SPI 在 ACPI 的 **off-by-32** 偏移）、global 暫存器位置。
2. **PCI Root Complex Node**：BCM2712 的 PCIe RC。
3. **ID Mapping Array**（寫在 RC node 內）：明確定義 PCIe RID `0x0000`~`0xFFFF` → 對應 SMMU node 的哪個 Stream ID。

> 另外 DSDT 的 `Device(PCI0)` 的 **`_CCA`**（cache coherency attribute）要設對——直接影響 DMA buffer 要不要 cache flush。

## 4. IORT 沒寫對的兩種後果（事實）

| 情況 | 條件 | 結果 |
|------|------|------|
| **A. SMMU bypass / identity map** | 硬體/UEFI 把 SMMU 設 bypass + IORT 缺失 | Windows 以為沒 IOMMU，WDF DMA 直接回 physical addr 當 logical。DMA **可能成功**——但若 RP1 只能 32-bit DMA 卻拿到 >4GB 實體位址 → DMA 失敗/覆寫錯誤記憶體 → BSOD |
| **B. SMMU fault/block（或 DMA Guard 開）** | SMMU 擋未映射交易 + IORT stream ID 對不上 | HAL 建了 IOVA 但寫錯 stream ID → DMA 到 SMMU 找不到 translation → 硬體擋下 + SMMU context fault → **BugCheck**（`DRIVER_VERIFIER_DMA_VIOLATION` / NMI/SError）|

- Win11 ARM64 因 HVCI，IOMMU 近乎強制；IORT 錯可能在 bootloader/HAL 極早期 panic，或 PCIe link up 後 load NVMe/USB driver 時死機。

## 5. 工程清單 + 最大未知數

**任務（全在 UEFI/EDK2）**：
1. UEFI 確保 SMMU clock/power 開、未被 ATF/TrustZone 鎖 secure-only；PCIe AXI 路由進 MMU-500（查 `pcie-brcmstb.c`/SMMU driver 有無 workaround）。
2. 從 `bcm2712.dtsi` 找 SMMU base/IRQ + PCIe RID→SID，依 DEN0049 寫 IORT。
3. 寫對 DSDT/MCFG（`_CCA` 等）。

**最大未知數**：RP1 內各周邊（USB vs Ethernet vs 相機）發 DMA 是**共用一個 PCIe BDF/RID 還是各自不同** → 決定 ID mapping 陣列。需查 Linux DT / 實機。

## 6. 參考 SOURCES
| 資源 | 用途 |
|------|------|
| **ARM DEN0049（IORT Specification）** | 寫 IORT table 的聖經 |
| Linux `arch/arm64/boot/dts/broadcom/bcm2712.dtsi` | SMMU node + PCIe iommu-map |
| Linux `drivers/iommu/arm/arm-smmu/arm-smmu.c` | 有無 bcm2712 quirks |
| MS Learn「ACPI for ARM64」+「Kernel DMA Protection」| Windows 端 IORT/DMA 要求 |
