# 顯示 DOD（rp1-vc4-dod，點亮 HDMI）實作規劃

> 路線見 [`MD/Note/gpu/`](../../../MD/Note/gpu/)（DOD 是 Phase 1：點亮畫面 + WARP，不做 3D）。
> 圖例：☐待 / ☑完成｜**[x64]** 本機 sim 可驗｜**[x64-build]** 可 /kernel 編譯｜**[Pi5]** 需 Win11 實機。

## Pi5 實機顯示真相（2026-06-25 SSH 萃取，BCM2712 SoC @ 0x107c000000）
| Block | 位址 | 備註 |
|-------|------|------|
| **HVS** | `0x107c580000` | Hardware Video Scaler；**在 BCM2712 IOMMU 後**（group 1）|
| **HDMI0 / HDMI1** | `0x107c701400` / `0x107c706400` | 兩個獨立 HDMI |
| **MOP / MOPLET** | `0x107c500000` / `0x107c501000` | Pi5 新輸出管線（vc4_txp_ops）|
| V3D（GPU render）| `0x1002000000` | 與顯示分離 |
| VideoCore 保留記憶體 | `mem_base=0x3fc00000 size=0x40000000` | mailbox/framebuffer |
| 現況 mode | `HDMI-A-1:2560x1440@60` | |

## 策略（見 gpu 筆記）：Hybrid — Mailbox 做 modeset/配 fb，MMIO 做 flip；最快「看到桌面」= 劫持 UEFI GOP framebuffer

---

## Phase A — KMDDOD 骨架（已完成）
- ☑ **A0** KMDDOD 註冊 + DDI stub（`driver.c`/`ddi.c`，`rp1vc4dod.sys` 編譯/link）。19 DDI 骨架、回報 1 個 HDMI child。

## Phase B — 純邏輯（x64 sim 可驗，推 🔵）
- ☑ **B1 [x64] VideoCore mailbox property-tag builder**（`vc_mailbox.c`）：**sim 過 + ARM64 /kernel 乾淨。**
  - `VcMboxInit/AddTag/Finalize` + `VcMboxSetPhysSize`(0x48003)/`VcMboxGetEdidBlock`(0x30020)；tag 常數含 alloc-buffer/set-virt/set-depth。
  - 驗：buffer 佈局（總 size、tag id、value-buf-size 對齊、end tag、req/resp、溢位回 0）。
- ☑ **B2 [x64] EDID parser**（`edid.c` `EdidParse`）：**sim 過 + ARM64 /kernel 乾淨。**
  - 驗 header + checksum；解第一個 DTD@54 → active W×H + pixel clock。驗：1920×1080/148500kHz；壞 header/checksum/短 buffer 被擋。
- ☐ **B3 [x64] hardcode 1080p EDID fallback** + mode 清單（讀不到 EDID 時用，可重用 B2 驗）。

## Phase C — DDI 接線（x64-build 可編譯；邏輯靠實機）
- ☑ **C1** `DodQueryDeviceDescriptor` 回 EDID（用 B3 `EdidGetDefault1080p`，處理 DescriptorOffset/Length 分塊）。**ARM64 /kernel 乾淨。**
- 🟡 **C2** VidPn 三劍客（`IsSupportedVidPn`/`EnumCofuncModality`/`RecommendFunctionalVidPn`）：目前 `IsSupportedVidPn` 回 TRUE 骨架；**完整 mode 列舉需 DXGK_VIDPN_INTERFACE 呼叫，偏 Stage D（實機）**。
- ☑ **C3** `DodCommitVidPn`：用 B1 `VcMboxSetPhysSize` 把 set-size mailbox 訊息建進 `dev->MboxBuf`（Stage D 送韌體 + 程式 HVS）。**ARM64 /kernel 乾淨。**
- 🟡 **C4** HVS dlist builder（`hvs_dlist.c` `HvsBuildCtl0`/`HvsBuildPos2`/`HvsBuildPlaneDlist`）：**sim 36/36 + ARM64 /kernel 乾淨**，用真實 `SCALER_CTL0`/`POS2` 欄位遮罩（gen5 序列待實機）。**剩**：`DodPresentDisplayOnly` flip 接線（memcpy 到 GOP fb / 寫 `SCALER_DISPLISTX`）= Stage D。
- ☐ **C5** `DdiInterruptRoutine`/軟體 timer VSync。

## Phase D — 實機（Pi5 Win11 ARM64）
- ☐ **D1** ACPI 描述顯示控制器 + framebuffer。
- ☐ **D2** M1 假顯示（RDP 看到 1080p PnP 螢幕）→ M2 軟體 VSync+WARP → **M3 UEFI GOP 劫持（看到桌面）** → M4 HVS 硬體 flip。

## 下一步（本機可做）
**B1 mailbox builder + B2 EDID parser + sim** → 把 DOD 的純邏輯推到 🔵；再進 Phase C DDI 接線（可編譯）。
