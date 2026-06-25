# `uefi_build/` — UEFI build 產出（丟 SD 卡取代原版）

> 這裡放 `uefi_fixed/build-uefi.sh` 產出的 **`RPI_EFI.fd`**（我們改版的 Pi5 UEFI 韌體）。
> `.fd` 等二進位**不納版控**（可由 `uefi_sources` + `uefi_fixed` 重 build），只追蹤本說明。

## 怎麼產生
```bash
wsl bash uefi_fixed/build-uefi.sh
# 完成後這裡會出現 RPI_EFI.fd
```

## 怎麼用（換到 SD 卡）
worproject 的開機 SD 卡（FAT32 開機分割區）通常含：`RPI_EFI.fd`、`config.txt`、
`bootcode`/`start*.elf`/`fixup*.dat`（Pi 韌體）、TF-A、`bcm2712-rpi-5-b.dtb`、`overlays/` 等。

1. **備份**原 SD 卡的 `RPI_EFI.fd`（例如複製成 `RPI_EFI.fd.orig`）。
2. 把本目錄的 `RPI_EFI.fd` **覆蓋**到 SD 卡開機分割區的 `RPI_EFI.fd`。
3. 其餘檔案（config.txt / Pi 韌體 / TF-A / dtb）**沿用原版不動**。
4. 插回 Pi5 開機；進 Windows 後裝置管理員應出現我們新描述的 RP1 周邊（`ACPI\RPI0GPIO` 等）。

## 注意
- 只換 `RPI_EFI.fd` 通常足夠（ACPI 表編在裡面）。若改了 TF-A 或記憶體 map 才需動其他檔。
- 出問題先用備份的 `RPI_EFI.fd.orig` 還原。
- 開發期若不想每次重 build/重刷，可改用 `asl.exe /loadtable` 動態注入 SSDT（見 `MD/Note/20260625-2230-...`）。
