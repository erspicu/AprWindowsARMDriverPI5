# 第 05 章：命脈——PCIe 與 RP1 列舉

> 目標：理解為什麼「打通 PCIe + RP1 列舉」是整個移植的先決條件，以及 `rp1bus.sys`（bus driver）做了什麼。

## 5.1 問題：RP1 的周邊「藏」在 PCIe 裡

RP1 對 PCIe 來說只是**一個 endpoint**（`VEN_1DE4 & DEV_0001`）。它內部那幾十個周邊
（USB/Ethernet/GPIO/I2C/SPI/PWM…）的暫存器，全部塞在這個 endpoint 的 **BAR1**（一塊 4MB 的 MMIO 視窗）裡。

```
PCIe 看到的：       1 個 endpoint（RP1）
RP1 裡面其實有：   USB×2, Ethernet, GPIO×3, I2C×7, SPI×8, UART×6, PWM, ADC, DMA, I2S×3, PIO …
全部在：           BAR1（實體 0x1F00000000, 4MB）裡的不同 offset
```

如果什麼都不做，Windows 只會看到「一張不認識的 PCIe 卡」。**沒有任何 RP1 周邊會出現在裝置管理員。**

## 5.2 三層架構

```
①  UEFI / ACPI  ──→  ECAM（MCFG 表）+ PNP0A08
                      讓 inbox pci.sys 能跑 PCIe，列舉到 RP1 endpoint
                                │
②  rp1bus.sys（自製 KMDF bus driver，綁 VEN_1DE4&DEV_0001）
       ├─ map BAR1（4MB MMIO 視窗）
       ├─ 為 RP1 內每個周邊建一個「子 PDO」，分配它在 BAR1 裡的 MMIO 區段
       └─ 註冊 GpioClx，把 RP1 的 61 條內部 IRQ（經 MSI-X）demux 給各子裝置
                                │
③  各子裝置驅動（rp1i2c / rp1gpio / rp1gem …）
       ├─ EvtPrepareHardware 拿到「自己那段 MMIO」→ MmMapIoSpaceEx
       └─ 中斷以 ACPI GpioInt（pin = RP1 內部 IRQ）連上 rp1bus 的 GpioClx
```

**白話**：`rp1bus.sys` 扮演「RP1 內部的分線盒」——把一塊大 MMIO 切成小段發給各驅動，
並把一束中斷拆開分流。

## 5.3 中斷怎麼走？RP1 內部 IRQ → MSI-X → GIC

- RP1 內部有自己的中斷彙整器（APBS @ BAR1+`0x108000`），把 61 個內部 IRQ 對應到 **61 條 MSI-X**。
- MSI-X 進到 BCM2712 的 GIC。
- `rp1bus.sys` 收到 MSI-X 後，看是哪條內部 IRQ，**demux** 給對應子裝置（透過 GpioClx 的中斷模型）。
- 子裝置在 ACPI 用 `GpioInt { N }` 宣告自己掛在 RP1 的第 N 條內部 IRQ。

```
RP1 周邊中斷 ─┐
              ├─ APBS 彙整（61 條）─→ MSI-X ─→ GIC ─→ rp1bus.sys ─→ demux ─→ 子裝置 ISR
RP1 周邊中斷 ─┘
```

> 為什麼用 GpioClx 做 demux？因為 GpioClx 的「一個控制器、多個中斷 pin」模型，剛好能表達
> 「rp1bus 是一個中斷來源、底下子裝置各佔一個 pin」。子裝置用 `GpioInt` 連上去，語意吻合。

## 5.4 子裝置怎麼拿到「自己那段 MMIO」？

bus driver 為子 PDO 提供記憶體資源，子裝置在 `EvtPrepareHardware` 取出並 map：

```c
NTSTATUS EvtPrepareHardware(WDFDEVICE dev, WDFCMRESLIST raw, WDFCMRESLIST trans) {
    for (ULONG i = 0; i < WdfCmResourceListGetCount(trans); i++) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR d = WdfCmResourceListGetDescriptor(trans, i);
        if (d->Type == CmResourceTypeMemory) {
            ctx->Mmio = MmMapIoSpaceEx(d->u.Memory.Start,
                                       d->u.Memory.Length,
                                       PAGE_READWRITE | PAGE_NOCACHE);
            ctx->MmioLen = d->u.Memory.Length;
        }
    }
    // 之後 HAL 就用 ctx->Mmio 當 void* Base 操作暫存器
}
```

→ 拿到的 `ctx->Mmio` 就是各章 HAL 裡的 `void* Base`。**HAL 完全不需要知道自己在 PCIe 底下**——
這正是「HAL 抽離」的好處（見 [第 06 章](06-porting-methodology.md)）。

## 5.5 為什麼「先做這個」？

| 沒有 PCIe+RP1 列舉 | 有了之後 |
|---|---|
| USB/網路/GPIO/I2C/SPI… 全看不到 | 子裝置能被 ACPI 列舉、各自載入驅動 |
| 任何 RP1 驅動都無從綁定 | HAL 拿得到 MMIO、中斷接得上 |

所以本專案的口號是 **「PCIe 是命脈」**：它是 Tier 0 裡最關鍵的先決 track。

## 5.6 本專案現況

- `windows_driver/pcie-rp1/rp1bus.sys`（ARM64 KMDF bus driver）：綁 `VEN_1DE4&DEV_0001`、map BAR1、
  列舉 RP1 內部周邊為子 PDO、註冊 GpioClx 做 MSI-X demux。
- `windows_sources/pcie-rp1/acpi/rp1.asl`：PNP0A08 + RP1 + ~25 個 RP1 子裝置 + 9 個 BCM2712 SoC 裝置。
- **Pi5 實測確認**：RP1 `1de4:0001` @ BAR1 `0x1F00000000`(4M)、MSI-X **61**、APBS@`0x108000`。

## 本章重點

- RP1 全部周邊的暫存器都在 PCIe **BAR1**（一塊 4MB MMIO）裡 → 不列舉就全看不到。
- 三層：**UEFI/ACPI 的 ECAM → `rp1bus.sys` 切 BAR1 + demux MSI-X → 各子裝置驅動**。
- 中斷：RP1 內部 IRQ → MSI-X → GIC → `rp1bus` demux；子裝置用 `GpioInt` 連上。
- 子裝置在 `EvtPrepareHardware` 拿到自己那段 MMIO，當 HAL 的 `void* Base`。

➡️ 下一章：[移植方法論：HAL 抽離 + OS glue + regio shim](06-porting-methodology.md)
