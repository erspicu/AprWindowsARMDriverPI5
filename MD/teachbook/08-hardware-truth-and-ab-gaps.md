# 第 08 章：用實機校正——Pi5 SSH 與「規格缺漏 A/B 兩類」

> 目標：理解「有 Linux 源碼 ≠ 所有規格都能從源碼確定」，學會用真 Pi5（跑 Linux）唯讀萃取硬體真相，
> 並用「**A/B 兩類缺漏**」這把尺，判斷某個值該「查源碼」還是「量實機」。

## 8.1 為什麼源碼不夠？源碼裝的是「演算法」，不是「數值」

驅動很多值是 **boot 時從活系統算/查出來的**，源碼本身不寫死。例：
```c
// Linux：MDC 分頻不是寫死，是依「實際 pclk」算
unsigned long pclk = clk_get_rate(bp->pclk);     // ← 值來自韌體設的 PLL
if (pclk <= 240000000) config = GEM_CLK_DIV96;   // 源碼只說「≤240MHz 用 DIV96」
```
源碼從沒說 pclk 是多少。**要知道它解析成 DIV96，得先知道 pclk = 200 MHz**——而那只能量實機。

## 8.2 兩類缺漏（核心判別法）

### A 類：本質 runtime 解析 —— 源碼只有公式，**值要量實機**
| 規格 | 源碼有的 | 源碼沒有的（量實機） |
|------|---------|----------------------|
| 時脈頻率 | `clk_get_rate()` + 分頻公式 | **實際 Hz**（韌體設 PLL）→ 量 `clk_summary` |
| 絕對實體位址 | DT 偏移 | **絕對位址**（PCIe 列舉時定）→ 讀 `/proc/iomem` |
| 中斷 GSIV | DT 中斷樹 | **最終 GIC 號**（boot 時算）→ `/proc/interrupts` |
| 實際生效配置 | 支援多板多變體 | **這塊板用哪顆**（SD=BCM2712? USB IRQ=31?）→ 看實機 |
| 硬體能力 | `readl(CAP)` | **晶片回傳值**（FIFO 深度/DBW）→ devmem 讀 |

→ 判別：源碼是 `xxx_get_rate()` / `platform_get_resource()` / `readl(cap)` → **A 類，量實機**。

### B 類：源碼有寫，但**移植時漏抄** —— 重讀源碼即可補
- 例：GEM 的 `GemConfigure` 原本只寫 `SPD|FD`，漏了 `macb_init_hw` 必設的 **MDC 分頻 + DRFCS**
  （後果：MDIO 的 MDC = pclk/8 = 25 MHz，爆掉 2.5 MHz 上限 → PHY 讀不到）。
- 判別：源碼有 `writel(該暫存器, ...)` 而我 HAL 沒有 → **B 類，重讀源碼補回**。

> **GEM 範例同時是 A+B**：源碼有 `gem_mdc_clk_div` 公式（B 的依據）＋我漏抄（B）＋實機定 pclk=200MHz（A）
> ＋devmem 讀回 `NCFGR=0x01560048` 的 CLK 欄位=DIV96（三方對上）→ 才敢確定。

## 8.3 連 Pi5 萃取硬體真相

有一台真 Pi5（跑 **Debian aarch64**，非 Windows），從 git-bash：
```sh
ssh -i ~/.ssh/aprvisual_pi -o BatchMode=yes pi@192.168.0.66 '<cmd>'
```
常用唯讀萃取：
```sh
lspci -vvv -s 0002:01:00.0          # RP1 PCIe BAR 大小/位址、MSI-X 數
sudo cat /proc/iomem | grep 1f00    # RP1 各周邊絕對位址
cat /proc/interrupts | grep rp1     # RP1 內部 IRQ 對應
sudo cat /sys/kernel/debug/clk/clk_summary   # 真實時脈頻率
sudo busybox devmem 0x1f00020000    # 讀某暫存器現值（驗 HAL 常數）
```

### ⚠️ 安全（重要）
- 這是**唯讀**萃取：`lspci`/`iomem`/`devtree`/`clk`/`devmem 讀`/`ftrace`。
- **勿 devmem 寫入**（干擾運作中周邊）；**勿讀未上電周邊**（可能 bus-error）。
- 別 unbind/rebind **eth0（SSH 命脈）/ mmc（rootfs）**——會斷線或毀檔。

## 8.4 為什麼不能直接在 Pi5-Linux「跑」驅動驗？

因為 Pi5 跑的是 **Linux**，載不了 Windows `.sys`。它只能給你**硬體真相**（量值、對照源碼），
不能驗 Windows 驅動的「功能」。最終 **🔵→✅** 仍需 **Pi5 + Win11-ARM** 實機載入 + KDNET 偵錯。

> 所以三種角色要分清：**x64 開發機**（寫碼+模擬）｜**Pi5-Linux**（硬體真相）｜**Pi5-Win11-ARM**（功能驗證）。

## 8.5 一個完整校正案例：GEM 的 MDC 分頻

1. **讀源碼**（B 依據）：`macb_init_hw` 有 `config = macb_mdc_clk_div(bp); ... NCFGR = config`。
2. **發現漏抄**（B）：我的 `GemConfigure` 只設 SPD|FD，沒設 CLK 欄位。
3. **量實機**（A）：`clk_summary` → GEM pclk = clk_sys = **200 MHz**。
4. **算**：200 MHz 落在 `gem_mdc_clk_div` 的「≤240MHz → DIV96(=5)」。
5. **devmem 驗**：讀回 `NCFGR=0x01560048`，CLK 欄位[20:18]=5=DIV96 ✓ 三方對上。
6. **補**：HAL 加 `NCFGR |= (5<<18) | DRFCS`。x64 sim 加斷言、跑綠。

→ 不校正的話，這個 bug 要在 Pi5 實機上「PHY 讀不到」時才會炸；現在在 x64 端就補好了。

## 本章重點

- **源碼=演算法，數值常 runtime 才定** → 不是所有規格都能從源碼確定。
- **A 類**（時脈/位址/GIC/能力）= 量實機；**B 類**（漏抄的暫存器設定）= 重讀源碼補。
- Pi5-Linux 給**硬體真相**（全程唯讀、別碰命脈周邊）；不能驗 Windows .sys。
- 校正流程：讀源碼 → 找漏抄(B) → 量實機(A) → 算 → devmem 驗 → 補 + sim。

➡️ 下一章：[ARM64 記憶體模型、MMIO barrier、驅動簽章](09-arm64-and-signing.md)
