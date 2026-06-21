# A6：BCM2712 GPIO（brcmstb，GpioClx）

> 和 [RP1 GPIO（B3）](B03-rp1-gpio.md) 不同 IP（brcmstb GIO），也教「**級聯中斷的 GSIV 怎麼算**」。

| | |
|---|---|
| **裝置** | BCM2712 GIO（內部/AON 訊號，如電源鍵；非 40-pin） |
| **Linux 源碼** | `drivers/gpio/gpio-brcmstb.c` |
| **Windows 框架** | **GpioClx** |
| **本專案** | `windows_sources/gpio/bcm2712-gpio/` ｜ INF `bcm2712gpio.inf` ｜ 狀態 🔵（sim 12/12） |

## 1. 與 RP1 GPIO 的差異

| | RP1 GPIO（B3） | BCM2712 GIO（本章） |
|---|---|---|
| 暫存器模型 | GPIO/RIO/PADS 三區 + atomic 別名 | 單一 GIO bank（ODEN/DATA/IODIR/EC/EI/MASK/LEVEL/STAT） |
| 用途 | 40-pin 排針 | 內部/always-on（電源鍵在這） |
| 中斷 | RP1 內部 IRQ→MSI-X | **級聯 L2-intc → GIC** |

GIO bank 暫存器（per-bank stride 0x20）：`IODIR`(1=輸入)、`DATA`、`EC`/`EI`(edge config)、`MASK`、`STAT`。
active = `STAT & MASK`。

## 2. 級聯中斷的 GSIV（重要 ACPI 技巧）

BCM2712 GIO 的中斷不是直接掛 GIC，而是**級聯**：
```
gpio.irq(0) → L2-intc@7d508400 (brcm,bcm7271-l2-intc) → GIC 244 → GSIV 276
```
所以 ACPI 描述這個 GPIO 的中斷，要填 **L2 控制器的 GIC 線換算出的 GSIV = 276**（DT SPI 244 + 32），
而不是 GPIO 自己的 pin 號。
> 怎麼查：Pi5 上 `dtc` 看 gpio 的 `interrupt-parent` phandle → 找到 L2-intc 節點 → 看它的 `interrupts`（GIC SPI）→ +32。

## 3. 移植

GpioClx callback 同 B3（read/write/方向/中斷），只是底層暫存器換成 GIO 模型：
```c
int BcmGpioReadPin(void *Base, unsigned pin) {
    unsigned b = pin / 32, off = pin % 32;
    return (RD32(Base, b*0x20 + GIO_DATA) >> off) & 1;
}
void BcmGpioSetDirection(void *Base, unsigned pin, int output) {
    // IODIR: 1=輸入、0=輸出（與 RP1 RIO OE 相反！）
    unsigned b=pin/32, off=pin%32, v=RD32(Base, b*0x20+GIO_IODIR);
    v = output ? (v & ~(1u<<off)) : (v | (1u<<off));
    WR32(Base, b*0x20 + GIO_IODIR, v);
}
```
> 踩雷：**IODIR 的極性與 RP1 相反**（1=輸入）。不同 IP 的 bit 語意要逐一確認，別憑印象。

➡️ 回 [目錄](README.md)
