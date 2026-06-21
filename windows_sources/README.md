# windows_sources — Windows on ARM 驅動原始碼

Raspberry Pi 5（BCM2712 + RP1）→ Windows on ARM (ARM64) 移植的**原始碼**根目錄，依**裝置類別**分子目錄。

## 每個驅動專案的慣用結構

```
<類別>/<專案>/
├─ <dev>_hw.h / <dev>_hw.c   # 純邏輯 HAL（void* Base + 暫存器序列，移植自 Linux）
├─ <dev>_regio.h             # 暫存器 I/O shim（真機用 READ/WRITE_REGISTER，sim 用 mock）
├─ driver.c (+ ddi.c / pnp.c)# OS 介接層（KMDF/GpioClx/SpbCx/SdPort/NDIS/PortCls… 重寫）
├─ common.h                  # 專案共用宣告
├─ sim/<dev>_sim.c           # x64 user-mode 模擬 harness（mock 暫存器 + 斷言）
├─ sim/<dev>_simshim.h       # 「fake kernel」shim（定義 sim 用型別/巨集）
└─ build.ps1                 # x64→ARM64 交叉編譯（直接呼叫 cl/link）
```

> 設計原則：HAL 不含 OS 政策（時脈/DMA/中斷/鎖），只有暫存器狀態機 → 可被 x64 sim 在無硬體下驗證。

## 子目錄（裝置類別）

| 目錄 | 裝置 / 框架 | 備註 |
|------|------------|------|
| `pcie-rp1/` | **RP1 PCIe bus driver**（KMDF）+ `acpi/rp1.asl` | 命脈：列舉 RP1 子裝置、MSI-X demux；ACPI 描述 RP1 + BCM2712 SoC 裝置 |
| `_common/` | 跨驅動共用程式碼 | |
| `i2c/` | RP1 DesignWare I2C、BCM2712 BSC I2C（SpbCx） | |
| `spi/` | RP1 DesignWare SPI、BCM2712 SPI（SpbCx） | |
| `gpio/` | RP1 GPIO、BCM2712 brcmstb GPIO（GpioClx） | |
| `audio/` | RP1 DesignWare I2S（WDM + PortCls/WaveRT） | |
| `net/` | RP1 GEM/macb Ethernet（NDIS） | |
| `storage/` | SDHCI（SdPort） | Pi5 SD/eMMC 實為 BCM2712 SoC |
| `dma/` | BCM2712 DMA（含 DMA40 40-bit 引擎） | |
| `pwm/` `adc/` `pio/` `clk/` `rng/` `mailbox/` `watchdog/` `rtc/` | RP1/BCM2712 各周邊（bespoke KMDF） | 多含 x64 sim |
| `bluetooth/` | BCM43438 H4（bthport） | |
| `wifi/` | CYW43455（NDIS，dot11/WDI 待補） | |
| `display/` | VC4 WDDM Display-Only Driver (DOD) | |
| `gpu/` | V3D WDDM render 骨架 | |
| `camera/` | RP1 CFE/PiSP（AVStream） | |

各驅動的移植狀態見 [`../MD/Note/RPi5-Porting-Status.md`](../MD/Note/RPi5-Porting-Status.md)。
