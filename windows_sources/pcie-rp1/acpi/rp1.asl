//
// rp1.asl - ACPI (SSDT) for RP1-over-PCIe on Windows on ARM (complete child set).
//
// Strategy (see MD/Note/20260621-0415-pcie-rp1-acpi-prereq-design.md):
//   * PCI0 = BCM2712 PCIe root complex (PNP0A08), enumerated by inbox pci.sys
//     from firmware ECAM/MCFG. _CRS here carries the bus-number range; the
//     memory windows come from firmware (MCFG/_CRS QWordMemory, board-tuned).
//   * RP1  = the RP1 PCI endpoint (_ADR dev1/func0). rp1bus.sys binds here,
//     slices BAR1 into per-peripheral MMIO (injected at PDO creation, NOT in
//     ACPI) and acts as a GpioClx interrupt controller over the 61 MSI-X IRQs.
//   * Children carry GpioInt resources whose pin == RP1 internal IRQ index,
//     pointing at the RP1 node (the GpioClx interrupt controller). _HID is the
//     reference binding ID; the bus driver may instead bind by HWID RP1\<name>.
//
// HID map (vendor "RPI5"): I2C=RPI50001 SPI=RPI50002 I2S=RPI50003
//   audio_out=RPI50004 PWM=RPI50005 GPIO=RPI50006 ETH=RPI50007 CSI=RPI50008
//   SDMMC=RPI50009 DMA=RPI5000A ADC=RPI5000B PIO=RPI5000C CLK=RPI5000D.
//   UART->ARMH0011 (inbox PL011),
//   USB->PNP0D10 (inbox xHCI).  RP1 internal IRQs per the BAR1 offset/IRQ table.
//
DefinitionBlock ("rp1.aml", "SSDT", 2, "RPI5", "RP1BUS", 0x00000001)
{
    Scope (\_SB)
    {
        Device (PCI0)                       // BCM2712 PCIe root complex
        {
            Name (_HID, EisaId ("PNP0A08")) // PCI Express
            Name (_CID, EisaId ("PNP0A03"))
            Name (_SEG, Zero)
            Name (_BBN, Zero)

            Method (_CRS, 0, NotSerialized)
            {
                // Bus-number range only; firmware supplies the ECAM/memory
                // windows via MCFG + its own host-bridge _CRS on real hardware.
                Name (RBUF, ResourceTemplate ()
                {
                    WordBusNumber (ResourceProducer, MinFixed, MaxFixed, PosDecode,
                        0x0000, 0x0000, 0x00FF, 0x0000, 0x0100)
                })
                Return (RBUF)
            }

            Device (RP1)                    // RP1 multi-function endpoint
            {
                Name (_ADR, 0x00010000)     // PCI device 1, function 0
                Name (_UID, 0x52503100)     // "RP1"

                // ===== UART0..5 -> inbox PL011 (SerCx2), IRQ 25,42,43,44,45,46 =====
                Device (URT0) { Name (_HID, "ARMH0011") Name (_UID, 0)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 25 } }) Return (RB) } }
                Device (URT1) { Name (_HID, "ARMH0011") Name (_UID, 1)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 42 } }) Return (RB) } }
                Device (URT2) { Name (_HID, "ARMH0011") Name (_UID, 2)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 43 } }) Return (RB) } }
                Device (URT3) { Name (_HID, "ARMH0011") Name (_UID, 3)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 44 } }) Return (RB) } }
                Device (URT4) { Name (_HID, "ARMH0011") Name (_UID, 4)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 45 } }) Return (RB) } }
                Device (URT5) { Name (_HID, "ARMH0011") Name (_UID, 5)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 46 } }) Return (RB) } }

                // ===== I2C0..6 -> rp1i2c (SpbCx), IRQ 7..13, 100kHz =====
                Device (I2C0) { Name (_HID, "RPI50001") Name (_UID, 0)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 7 } }) Return (RB) }
                    Name (_DSD, Package () { ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                        Package () { Package () { "clock-frequency", 0x000186A0 } } }) }
                Device (I2C1) { Name (_HID, "RPI50001") Name (_UID, 1)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 8 } }) Return (RB) } }
                Device (I2C2) { Name (_HID, "RPI50001") Name (_UID, 2)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 9 } }) Return (RB) } }
                Device (I2C3) { Name (_HID, "RPI50001") Name (_UID, 3)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 10 } }) Return (RB) } }
                Device (I2C4) { Name (_HID, "RPI50001") Name (_UID, 4)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 11 } }) Return (RB) } }
                Device (I2C5) { Name (_HID, "RPI50001") Name (_UID, 5)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 12 } }) Return (RB) } }
                Device (I2C6) { Name (_HID, "RPI50001") Name (_UID, 6)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 13 } }) Return (RB) } }

                // ===== SPI0..5 -> rp1spi (SpbCx), IRQ 19..24 =====
                Device (SPI0) { Name (_HID, "RPI50002") Name (_UID, 0)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 19 } }) Return (RB) } }
                Device (SPI1) { Name (_HID, "RPI50002") Name (_UID, 1)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 20 } }) Return (RB) } }
                Device (SPI2) { Name (_HID, "RPI50002") Name (_UID, 2)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 21 } }) Return (RB) } }
                Device (SPI3) { Name (_HID, "RPI50002") Name (_UID, 3)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 22 } }) Return (RB) } }
                Device (SPI4) { Name (_HID, "RPI50002") Name (_UID, 4)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 23 } }) Return (RB) } }
                Device (SPI5) { Name (_HID, "RPI50002") Name (_UID, 5)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 24 } }) Return (RB) } }

                // ===== I2S0..2 -> rp1i2saud (PortCls), IRQ 14,15,16 =====
                Device (I2S0) { Name (_HID, "RPI50003") Name (_UID, 0)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 14 } }) Return (RB) } }
                Device (I2S1) { Name (_HID, "RPI50003") Name (_UID, 1)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 15 } }) Return (RB) } }
                Device (I2S2) { Name (_HID, "RPI50003") Name (_UID, 2)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 16 } }) Return (RB) } }

                // ===== analog audio out -> PortCls, IRQ 4 =====
                Device (AUD0) { Name (_HID, "RPI50004") Name (_UID, 0)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 4 } }) Return (RB) } }

                // ===== PWM0/1 -> rp1pwm (KMDF), IRQ 5,41 =====
                Device (PWM0) { Name (_HID, "RPI50005") Name (_UID, 0)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 5 } }) Return (RB) } }
                Device (PWM1) { Name (_HID, "RPI50005") Name (_UID, 1)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 41 } }) Return (RB) } }

                // ===== ADC (SAR) -> rp1adc (KMDF), reg @BAR1+0xC8000 (Pi5-confirmed).
                // One-shot polled (READY bit) -> no interrupt resource needed. =====
                Device (ADC0) { Name (_HID, "RPI5000B") Name (_UID, 0)
                    Method (_CRS) { Name (RB, ResourceTemplate () { }) Return (RB) } }

                // ===== PIO (programmable IO) -> rp1pio (KMDF), reg @BAR1+0x178000.
                // FIFO push/pop (SM programs via firmware) -> no interrupt here. =====
                Device (PIO0) { Name (_HID, "RPI5000C") Name (_UID, 0)
                    Method (_CRS) { Name (RB, ResourceTemplate () { }) Return (RB) } }

                // ===== clocks (PLL/clock-gen) -> rp1clk (KMDF), reg @BAR1+0x18000.
                // Clock provider (no IRQ); PLL_SYS verified locked on Pi5. =====
                Device (CLK0) { Name (_HID, "RPI5000D") Name (_UID, 0)
                    Method (_CRS) { Name (RB, ResourceTemplate () { }) Return (RB) } }

                // ===== GPIO (3 banks) -> rp1gpio (GpioClx), IO_BANK0/1/2 = IRQ 0,1,2 =====
                Device (GPIO) { Name (_HID, "RPI50006") Name (_UID, 0)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 0 }
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 1 }
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 2 } }) Return (RB) } }

                // ===== Ethernet (Cadence GEM) -> rp1gem (NDIS), IRQ 6 =====
                Device (ETH0) { Name (_HID, "RPI50007") Name (_UID, 0)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 6 } }) Return (RB) } }

                // ===== camera CSI0/1 -> rp1cfe (AVStream), MIPI0/1 = IRQ 47,48 =====
                Device (CSI0) { Name (_HID, "RPI50008") Name (_UID, 0)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 47 } }) Return (RB) } }
                Device (CSI1) { Name (_HID, "RPI50008") Name (_UID, 1)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 48 } }) Return (RB) } }

                // NOTE: SD/eMMC are NOT RP1 peripherals. On Pi5 they are BCM2712
                // SoC controllers (mmc0 eMMC @0x1000FFF000 GIC305, mmc1 SD-slot
                // @0x1001100000 GIC306) -> moved to \_SB.SDC0/SDC1 below (fixed
                // MMIO + GIC, not PCIe BAR demux). Pi5-measured 2026-06-21.

                // ===== DMA (dw-axi-dmac) -> rp1dma (KMDF DMA), IRQ 40 =====
                Device (DMA0) { Name (_HID, "RPI5000A") Name (_UID, 0)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 40 } }) Return (RB) } }

                // ===== USB0/1 (dwc3 host) -> inbox xHCI (PNP0D10), IRQ 31,36 =====
                // (Pi5-measured: /proc/interrupts rp1_irq_chip xhci-hcd:usb1=31, usb3=36)
                Device (USB0) { Name (_HID, "PNP0D10") Name (_UID, 0)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 31 } }) Return (RB) } }
                Device (USB1) { Name (_HID, "PNP0D10") Name (_UID, 1)
                    Method (_CRS) { Name (RB, ResourceTemplate () {
                        GpioInt (Level, ActiveHigh, Exclusive, PullNone, 0, "\\_SB.PCI0.RP1") { 36 } }) Return (RB) } }
            }
        }

        // ===== BCM2712 SoC SD/eMMC -> rp1sd (SdPort). Fixed MMIO + GIC SPI.
        // Pi5-measured: mmc0 eMMC host@0x1000FFF000(+cfg 0x400), DT SPI 273 -> GSIV 305;
        //               mmc1 SD-slot host@0x1001100000(+cfg 0x400), DT SPI 274 -> GSIV 306;
        // base clock = emmc2-clock = 200 MHz. (NOT RP1 children.)
        Device (SDC0)                       // eMMC
        {
            Name (_HID, "RPI50009")
            Name (_UID, 0)
            Method (_CRS, 0, NotSerialized) { Name (RB, ResourceTemplate () {
                QWordMemory (ResourceConsumer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                    0x0, 0x0000001000FFF000, 0x0000001000FFF5FF, 0x0, 0x0000000000000600)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 305 }
            }) Return (RB) } }
        Device (SDC1)                       // SD card slot
        {
            Name (_HID, "RPI50009")
            Name (_UID, 1)
            Method (_CRS, 0, NotSerialized) { Name (RB, ResourceTemplate () {
                QWordMemory (ResourceConsumer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                    0x0, 0x0000001001100000, 0x00000010011005FF, 0x0, 0x0000000000000600)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 306 }
            }) Return (RB) } }

        // ===== BCM2712 SoC peripherals (fixed MMIO + GIC; Pi5-measured /proc/iomem).
        // HID map: mailbox=BCM2EB0 rng=BCM2EB1 wdt=BCM2EB2 dma=BCM2EB3. =====
        Device (MBOX)                       // VideoCore mailbox -> bcmmbox
        {
            Name (_HID, "BCM2EB0") Name (_UID, 0)
            Method (_CRS, 0, NotSerialized) { Name (RB, ResourceTemplate () {
                QWordMemory (ResourceConsumer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                    0x0, 0x000000107C013880, 0x000000107C0138BF, 0x0, 0x0000000000000040)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 65 }
            }) Return (RB) } }
        Device (RNG0)                       // iproc-rng200 (polled FIFO) -> bcmrng
        {
            Name (_HID, "BCM2EB1") Name (_UID, 0)
            Method (_CRS, 0, NotSerialized) { Name (RB, ResourceTemplate () {
                QWordMemory (ResourceConsumer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                    0x0, 0x000000107D208000, 0x000000107D208027, 0x0, 0x0000000000000028)
            }) Return (RB) } }
        Device (WDT0)                       // PM watchdog (no IRQ) -> bcmwdt
        {
            Name (_HID, "BCM2EB2") Name (_UID, 0)
            Method (_CRS, 0, NotSerialized) { Name (RB, ResourceTemplate () {
                QWordMemory (ResourceConsumer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                    0x0, 0x000000107D200000, 0x000000107D200307, 0x0, 0x0000000000000308)
            }) Return (RB) } }
        Device (DMAC)                       // bcm2835-dma (legacy) -> bcmdma
        {
            Name (_HID, "BCM2EB3") Name (_UID, 0)
            Method (_CRS, 0, NotSerialized) { Name (RB, ResourceTemplate () {
                QWordMemory (ResourceConsumer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                    0x0, 0x0000001000010000, 0x0000001000010BFF, 0x0, 0x0000000000000C00)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 119, 120, 121, 122 }
            }) Return (RB) } }

        // ===== BCM2712 brcmstb GPIO (GpioClx) -> bcm2712gpio. HID BCM2EB5.
        // bank0 @0x107D508500 (irq-capable: cascades gpio.irq0 -> l2-intc@7d508400
        // -> GIC 244 -> GSIV 276); bank1 @0x107D517C00 (IO-only). Pi5-measured. =====
        Device (BGIO)
        {
            Name (_HID, "BCM2EB5") Name (_UID, 0)
            Method (_CRS, 0, NotSerialized) { Name (RB, ResourceTemplate () {
                QWordMemory (ResourceConsumer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                    0x0, 0x000000107D508500, 0x000000107D50853F, 0x0, 0x0000000000000040)
                QWordMemory (ResourceConsumer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                    0x0, 0x000000107D517C00, 0x000000107D517C3F, 0x0, 0x0000000000000040)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 276 }
            }) Return (RB) } }

        // ===== Raspberry Pi firmware RTC -> rpirtc. HID BCM2EB6. No MMIO:
        // time get/set proxied via VideoCore mailbox (#16). (rpi_rtc on Pi5.) =====
        Device (RTC0)
        {
            Name (_HID, "BCM2EB6") Name (_UID, 0)
            Method (_CRS, 0, NotSerialized) { Name (RB, ResourceTemplate () { }) Return (RB) } }

        // ===== BCM2712 BSC I2C -> bcm2712i2c (SpbCx). HID BCM2EA1. Pi5-measured:
        // i2c0 @0x107D508200, i2c1 @0x107D508280; both cascade L2-intc@7d508380
        // (GIC 242) -> GSIV 274. =====
        Device (BSC0)
        {
            Name (_HID, "BCM2EA1") Name (_UID, 0)
            Method (_CRS, 0, NotSerialized) { Name (RB, ResourceTemplate () {
                QWordMemory (ResourceConsumer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                    0x0, 0x000000107D508200, 0x000000107D508257, 0x0, 0x0000000000000058)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 274 }
            }) Return (RB) } }
        Device (BSC1)
        {
            Name (_HID, "BCM2EA1") Name (_UID, 1)
            Method (_CRS, 0, NotSerialized) { Name (RB, ResourceTemplate () {
                QWordMemory (ResourceConsumer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                    0x0, 0x000000107D508280, 0x000000107D5082D7, 0x0, 0x0000000000000058)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 274 }
            }) Return (RB) } }

        // ===== BCM2712 SPI -> bcm2712spi (SpbCx). HID BCM2EA2. Pi5-measured:
        // @0x107D004000, GIC 118 -> GSIV 150; core clock = vpu-clock 750 MHz. =====
        Device (SPI0)
        {
            Name (_HID, "BCM2EA2") Name (_UID, 0)
            Method (_CRS, 0, NotSerialized) { Name (RB, ResourceTemplate () {
                QWordMemory (ResourceConsumer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                    0x0, 0x000000107D004000, 0x000000107D0041FF, 0x0, 0x0000000000000200)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 150 }
            }) Return (RB) } }
    }
}
