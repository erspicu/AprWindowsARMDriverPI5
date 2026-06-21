# Skill：Pi5 實機（Linux）SSH — 硬體真相萃取

> 操作型 how-to。基本守則見 `../../CLAUDE.md`。

## 連線
有一台真 Pi5（BCM2712，跑 **Debian 13 aarch64**，**非 Windows**）：
```sh
ssh -i ~/.ssh/aprvisual_pi -o BatchMode=yes pi@192.168.0.66 '<cmd>'
```
- 從 **git-bash** 跑；私鑰 `C:\Users\erspi\.ssh\aprvisual_pi`（空通關密語）。
- **IP 非固定**（`192.168.0.66`，重開機可能變）；`sudo -n` 免密碼。
- 連線逾時偶發，重試即可。

## 用途與界線
- **用途＝硬體真相**（唯讀）：`lspci -vvv` / `/proc/iomem` / `dtc`(device-tree) / `clk_summary` /
  `devmem`(busybox) / `ftrace`。
- **不能跑/驗證 Windows `.sys`**（Pi5 跑 Linux）；最終 🔵→✅ 仍需 Pi5 + Win11-ARM。

## ⚠️ 安全（重要）
- 此為使用者 **benchmark 機，全程唯讀**。
- **勿 devmem 寫入**（會干擾正在用的周邊）；**勿讀未上電/未啟用周邊**（可能 bus-error）。
- 開 ftrace event 後**記得關回**（`echo 0 > .../enable`）。
- 重追 init 序列要 unbind/rebind → **eth0(SSH 命脈)/mmc(rootfs) 絕對不可**碰。

## 規格缺漏 A/B 兩類（判別原則，重要）
源碼≠所有規格都能從源碼確認：
- **A 類**＝runtime 才解析（**只能量實機**）：時脈頻率(`clk_get_rate`)、絕對實體位址(PCIe 列舉)、
  中斷 GSIV(中斷樹)、實際生效配置、硬體能力暫存器(`readl(cap)`)。
- **B 類**＝**源碼有寫但移植時漏抄**（**重讀源碼即可補**，實機只作確認）：缺的 config bit / 分頻 /
  序列。判別：源碼是 `xxx_get_rate()`/`platform_get_resource()`/`readl(cap)` → A 類；源碼有 `writel(該暫存器)`
  而我 HAL 沒有 → B 類。

## 已萃取的硬體事實（摘要）
RP1 PCIe `1de4:0001` @ BAR1 `0x1f00000000`(4M)、MSI-X 61；周邊偏移全中（mailbox@8000/clocks@18000/
pwm@9c000/adc@c8000/gpio×3@d0000-f0000/eth@100000/pio@178000/dma@188000/usb@200000-300000）；
IRQ eth=6·dma=40·mailbox=58·usb=31/36；**clk_sys=200MHz**（RP1 I2C/SPI/SDIO 分頻基準）、
audio PLL=61.44MHz、BCM SPI=750MHz、BCM I2C=108MHz；BCM2712 SoC：SD/eMMC `0x1000FFF000`/`0x1001100000`
(GIC305/306)、mailbox `0x107C013880`(GIC65) 等。

完整事實表 + A/B 原則全文：`../Note/20260621-0720-pi5-linux-hardware-facts.md`。
源碼 vs 移植的 B 類缺漏補正紀錄：`../Note/20260621-0820-source-vs-port-bclass-gaps.md`。
