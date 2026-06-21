# B10：RP1 PWM ×2（bespoke KMDF）

> 第一個 **bespoke KMDF** 範例——沒有現成 Cx 框架的小周邊，就用裸 KMDF + 自訂介面。
> PWM 也是「**漏一個鎖存位元，設定就完全不生效**」的好教材。

| | |
|---|---|
| **裝置** | RP1 PWM（×2，常用於散熱風扇 PWM1） |
| **Linux 源碼** | `drivers/pwm/pwm-rp1.c` |
| **Windows 框架** | **bespoke KMDF**（自訂 IOCTL/介面） |
| **本專案** | `windows_sources/pwm/rp1-pwm/` ｜ INF `rp1pwm.inf`（`ACPI\RPI50005`） |
| **狀態** | 🔵 邏輯完整（x64 sim 11/11） |

## 1. bespoke KMDF 模板

小周邊沒有 SpbCx/GpioClx 這種代勞框架，就用最基本的 KMDF：
```
common.h     # ntddk + wdf + device context
driver.c     # DriverEntry → WdfDriverCreate；EvtDeviceAdd 設 callback；
             # EvtPrepareHardware 找 CmResourceTypeMemory → MmMapIoSpaceEx → ctx->Base
<dev>_hw.h/c # clean void* HAL + regio shim
sim/         # x64 模擬
build.ps1
```
對外可用自訂 IOCTL 讓 user-mode 設 duty/period（或內部給風扇熱區用）。

## 2. 規格 / 暫存器重點

| 暫存器 | offset | 用途 |
|--------|-------:|------|
| `GLOBAL_CTRL` | 0x000 | 各 channel enable + **SET_UPDATE(bit31)** |
| `CHANNEL_CTRL(x)` | 0x014+x*16 | 模式（trailing-edge M/S）、FIFO_POP、極性 |
| `RANGE(x)` | 0x018+x*16 | 週期（clock 數） |
| `DUTY(x)` | 0x020+x*16 | 工作週期（clock 數） |

## 3. 經典案例：SET_UPDATE（不寫 → 設定完全不生效）

PWM 的 duty/range 是**先暫存、再一次鎖存生效**。源碼每次 apply 後都寫 `GLOBAL_CTRL |= SET_UPDATE`：
```c
void PwmHwSetDutyRange(void *Base, unsigned ch, unsigned duty, unsigned range) {
    WR32(Base, PWM_DUTY(ch),  duty);
    WR32(Base, PWM_RANGE(ch), range);
    PwmHwApplyConfig(Base);        // ← 關鍵：把暫存值鎖存生效
}
void PwmHwApplyConfig(void *Base) {
    WR32(Base, PWM_GLOBAL_CTRL, RD32(Base, PWM_GLOBAL_CTRL) | PWM_SET_UPDATE);
}
```
> **漏了會怎樣**：你設了 duty/range，暫存器也寫進去了，但 **PWM 輸出完全不變**——因為沒鎖存。
> 這種 bug 在實機上很難一眼看出（暫存器值看起來都對），是「B 類」靠重讀源碼才補得回的典型。

## 4. 移植要點

- duty/range 是 **clock 數**，不是百分比/秒——caller 要用「PWM 輸入時脈週期」換算（A 類：時脈量實機）。
- enable 也走 `GLOBAL_CTRL`，本專案把 enable 與 SET_UPDATE 合併一次寫。
- 極性（CHANNEL_CTRL bit3）反相輸出可選。

## 5. x64 模擬

```
[PASS] CH0 DUTY == 50   [PASS] CH0 RANGE == 100
[PASS] SET_UPDATE latched after SetDutyRange     ← 驗證鎖存有寫
[PASS] GLOBAL_CTRL bit0 set (CH0 enabled)
== 11 passed, 0 failed ==
```

➡️ 回 [目錄](README.md)
