# SpbCx I2C/SPI 批次 + 自動移植 loop（第二輪）總結

> 日期：2026-06-21 05:00　｜　loop job `be74294d` 本筆記後解除

## 本輪 loop 產出（4 個 SpbCx 驅動，全 ARM64 build 成功）
| 驅動 | 產出 | HAL 來源 |
|------|------|----------|
| RP1 I2C（DesignWare） | `windows_driver/i2c/rp1i2c.sys` | `i2c-designware-*` |
| RP1 SPI（DesignWare） | `windows_driver/spi/rp1spi.sys` | `spi-dw-*` |
| BCM2712 I2C（BSC） | `windows_driver/i2c/bcm2712i2c.sys` | `i2c-bcm2835.c` |
| BCM2712 SPI | `windows_driver/spi/bcm2712spi.sys` | `spi-bcm2835.c` |

**SpbCx pattern 一旦建立，換 HAL 暫存器層即可快速複製**（4 個驅動，後 3 個都一次 build 成功）。詳細 SpbCx know-how 見 `20260621-0430-rp1-i2c-spbcx.md`。

## 重要決策
- **UART（PL011）不自寫**：Windows 有 inbox `SerPl011.sys`，綁 ACPI `_HID ARMH0011` 即載入 → 標為「免驅動」。SerCx2 自訂驅動多餘（AXI 變體待實機確認）。
- **GpioClx（GPIO）跳過**：`GPIO_CLIENT_REGISTRATION_PACKET` 約 18 個 callback、且與中斷 demux 緊密綁定，非一輪可乾淨 build → 列入「需專門處理」。

## 為何在此停止 loop（「能移植的移植完」）
可乾淨移植的**標準 Windows 框架**批次已完成：
- SpbCx（I2C/SPI ×4）✅、PortCls（音訊）✅、WDDM DOD（顯示）✅、KMDF bus（PCIe/RP1）✅、UART→inbox ✅

剩下的 ⬜ 項目皆屬「需特別處理」tier，逐項與使用者討論：
1. **大型多-callback 框架（需多輪careful工）**：GpioClx（GPIO）、sdport（SD/MMC）、AVStream（相機/CSI）、完整 WDDM render（V3D GPU）、NDIS（Ethernet）。
2. **需實機/韌體**：WiFi(brcmfmac)、USB3(dwc3)、Ethernet PHY、RP1 PCIe 端到端列舉、各驅動實機驗證。
3. **bespoke 自訂 KMDF（無標準框架，價值偏低）**：PWM、clocks、mailbox、ADC、PIO、DMA — 可做可建置骨架但 Windows 無對應 class。
4. **韌體相依**：VideoCore 韌體介面、RP1 韌體、power/RTC。

## 全專案累計（🟢 8 個 build + 2 scaffold）
音訊 `rp1i2s`/`rp1i2saud`、顯示 `rp1vc4dod`、PCIe/RP1 `rp1bus`、I2C `rp1i2c`/`bcm2712i2c`、SPI `rp1spi`/`bcm2712spi`；GPU `rp1-v3d`(scaffold)、ACPI `rp1.aml`。

## 恢復方式
重啟 `/loop` 或指定 track。建議下一階段優先序由使用者選：GpioClx（解鎖 GPIO+中斷）、或補完 PCIe bus 的 raw-resource/GpioClx demux（讓已建的 I2C/SPI 在實機真正拿到資源+中斷）。
