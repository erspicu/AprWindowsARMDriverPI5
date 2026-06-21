# RP1 GPIO（GpioClx）scaffold know-how

> 日期：2026-06-21 05:15　｜ 專案 `windows_sources/gpio/rp1-gpio/`

## 狀態
- **scaffold 編譯通過（ARM64）**：`DriverEntry` + `GPIO_CLX_RegisterClient` + 註冊封包，16 個 client callback 用 WDK 函式 typedef 前向宣告（簽章鎖定）。
- callback 實作 + RP1 GPIO/RIO HAL + link 為下一步。

## GpioClx know-how
- 標頭 `km\gpioclx.h`；lib **`msgpioclxstub.lib`**（`Lib\...\km\arm64\`）；KMDF-based。
- `pkt.Version = GPIO_CLIENT_VERSION`（= **0x4**）；`pkt.Size = sizeof`；`pkt.ControllerContextSize = sizeof(自訂 context)`。
- `DriverEntry`：`WdfDriverCreate` → 填 `GPIO_CLIENT_REGISTRATION_PACKET` → `GPIO_CLX_RegisterClient(driver, &pkt, RegistryPath)`。
- `EvtDeviceAdd`：`GPIO_CLX_ProcessAddDevicePreDeviceCreate(Driver, DeviceInit, &fdoAttr)` → `WdfDeviceCreate` → `GPIO_CLX_ProcessAddDevicePostDeviceCreate(Driver, Device)`。
- 16 必填 callback（函式 typedef 名 = 封包欄位型別去掉 P 前綴）：PrepareController/Release/Start/Stop、QueryControllerBasicInformation、QuerySetControllerInformation、Enable/Disable/Unmask/Mask/QueryActive/ClearActive Interrupt(s)、Connect/DisconnectIoPins、Read/WriteGpioPins。可 NULL：ReadGpioPinsUsingMask、WriteGpioPinsUsingMask、Save/RestoreBankHardwareContext、PreProcessControllerInterrupt、ControllerSpecificFunction、ReconfigureInterrupt。

## 下一步
1. 抽各 callback 的 params struct（`GPIO_CONTROLLER_BASIC_INFORMATION`、`GPIO_READ_PINS_PARAMETERS`、`GPIO_WRITE_PINS_PARAMETERS`、`GPIO_CONNECT_IO_PINS_PARAMETERS`、`GPIO_ENABLE_INTERRUPT_PARAMETERS`…）→ 實作 16 callback。
2. RP1 GPIO/RIO HAL（pinctrl-rp1：GPIO@0xd0000 三 bank；RIO set/clr/oe；pads；status/ctrl per pin）。
3. link `msgpioclxstub.lib` → `rp1gpio.sys`。
4. 接 #4：本驅動同時做 **RP1 內部中斷 demux**（MSI-X→GpioClx pin），讓 rp1bus 子裝置的 GpioInt 生效。
