# WHD Windows Port Layer

> Windows-kernel porting layer for Infineon **WHD (Wi-Fi Host Driver)**, the route
> chosen for CYW43455 WiFi (see [`../PLAN.md`](../PLAN.md) + [`MD/Note/wifi/`](../../../../MD/Note/wifi/)).
> WHD upstream is cloned read-only at `sources/wifi-host-driver/` (gitignored, not vendored).

## WHD integration surface (verified against the cloned WHD source)

WHD is a pure-C library you drive top-down and that calls **down** into two things we provide:

```
whd_init(&drv, &cfg, &buffer_ops, &network_ops, &resource_ops)   // top entry
whd_bus_sdio_attach(drv, &sdio_cfg, &sdio_obj)                    // bind SDIO
whd_wifi_on(drv, &ifp)                                           // load firmware
   │
   ├─ cy_rtos_*    (OS abstraction)  ── we implement → kernel primitives
   └─ cyhal_sdio_* (SDIO HAL)        ── we implement → SDIO_OPS → sdbus.sys
```

## What this folder implements (all ARM64 `/kernel`-compile-clean)

| File | Provides | Maps to | Status |
|------|----------|---------|--------|
| `whd_port.h` | Windows `cy_rtos_*` types (`cy_semaphore_t`=KSEMAPHORE, `cy_mutex_t`=KMUTEX, `cy_thread_t`=PETHREAD, `cy_timer_t`=KTIMER+KDPC) + prototypes | — | ✅ compiles |
| `cy_rtos_win.c` | the **cy_rtos_* subset WHD uses** (semaphore/mutex/thread/timer/delay/time) | `KeInitializeSemaphore`/`KeWaitForSingleObject`/`KeReleaseSemaphore`, `KeInitializeMutex`, `PsCreateSystemThread`, `KeSetTimerEx`+DPC, `KeDelayExecutionThread`, `KeQueryInterruptTime` | ✅ compiles, **PASSIVE_LEVEL**; logic untested (needs target) |
| `cyhal_sdio_win.c` | `cyhal_sdio_send_cmd` (CMD52) / `cyhal_sdio_bulk_transfer` (CMD53) | decode SD argument → `SDIO_OPS` (`../sdio_core.c`) | ✅ compiles; forwards to SDIO_OPS |
| `sdbus_glue.c` / `.h` | `SDIO_OPS` Cmd52/Cmd53 over the **real inbox sdbus.sys** (`SDBUS_REQUEST_PACKET` + `SdBusSubmitRequest`, `<ntddsd.h>`/`<sddef.h>`) + `WifiSdioReadChipId` | `WdfFdoQueryForInterface(GUID_SDBUS_INTERFACE_STANDARD)` → `SDRF_DEVICE_COMMAND` + `SDCMD_DESCRIPTOR` | ✅ **ARM64 /kernel-clean**; live path needs target |

> Note: the SD function-driver interface (`SDBUS_INTERFACE_STANDARD` / `SDBUS_REQUEST_PACKET` /
> `SdBusSubmitRequest`) lives in **`km\ntddsd.h`** (+ `SDCMD_DESCRIPTOR` in `km\sddef.h`) on this WDK —
> NOT the old `sdbus.h`. It uses `SDRF_DEVICE_COMMAND` + a raw SD command descriptor (CMD52/CMD53),
> not the WDK8-era `SDRF_READ_PORT` style.

WHD-used `cy_rtos_*` (from grep on WHD/src): `init/get/set/deinit_semaphore`,
`init/get/set/deinit_mutex`, `create/join/exit_thread`, `init/start/stop/deinit/is_running_timer`,
`delay_milliseconds`, `get_time`.

## The remaining piece (Win11/ARM64 target only)

`SDIO_OPS.Cmd52/Cmd53` is the **injection point**. On hardware it is wired to inbox
**`sdbus.sys`** (`SDBUS_REQUEST_PACKET` + `SubmitRequest`, PASSIVE_LEVEL synchronous) +
the in-band SDIO interrupt callback. That glue + `whd_init`/`whd_bus_sdio_attach`/`whd_wifi_on`
wiring + firmware/NVRAM load is Phase B/C — it needs the Pi5 running Win11 ARM64 to test.

## Final integration notes (mechanical)
- Replace the local `cy_*` types / `cyhal_sdio_t` here with WHD's upstream
  `External/rtos/cyabs_rtos.h` (+ a `cyabs_rtos_impl.h` aliasing to our structs) and
  `External/hal/cyhal_sdio.h`, so signatures bind exactly. Our prototypes already mirror them.
- Firmware (Pi5, confirmed on device): `cyfmac43455-sdio.bin` + `.clm_blob` + NVRAM
  `brcmfmac43455-sdio.txt` (preprocess via `WhdNvramPreprocess` in `../sdio_core.c`).
- Chip-id bring-up (`Cyw43455ReadChipId`) verified vs real Pi5: F1 sig `0x15264345`.
