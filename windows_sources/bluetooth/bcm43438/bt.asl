/*
 * bt.asl - ACPI device fragment for the Pi5 (BCM4345C0) UART Bluetooth.
 *
 * DRAFT for the Pi5 Win11/ARM64 UEFI DSDT. Wires the BT chip to a UART + reset
 * GPIO so Windows PnP loads the transport. Pi5 facts (from Linux DT, see
 * MD/Note/20260625-0030-...): UART = BCM2712 PL011 (brcm,bcm7271-uart) at
 * serial@7d50c000; reset = shutdown-gpios GPIO29; operational baud 3,000,000.
 *
 * Confirm on target: the ACPI device path of the PL011 (ResourceSource below),
 * the GPIO controller path + pin, and the _HID (must match the INF HardwareId
 * and be assigned by the UEFI). The chip BOOTS at 115200; the driver raises the
 * host baud to 3M after firmware load, so the _CRS initial speed is 115200.
 */

Scope (\_SB)
{
    Device (BTH0)
    {
        Name (_HID, "BCM2EA6")          // placeholder ACPI id; must match the INF
        Name (_CID, "BCM2EA6")
        Name (_UID, 0)
        Method (_STA, 0, NotSerialized) { Return (0x0F) }

        Method (_CRS, 0, NotSerialized)
        {
            Name (RBUF, ResourceTemplate ()
            {
                // H4 HCI UART (initial 115200 8N1, HW flow control).
                // ResourceSource must name the BCM2712 PL011 ACPI device.
                UARTSerialBusV2 (115200, DataBitsEight, StopBitsOne, 0xFC,
                    LittleEndian, ParityTypeNone, FlowControlHardware, 16, 16,
                    "\\_SB.URT0", 0, ResourceConsumer, , )

                // BT_REG_ON (shutdown-gpios, active high) -> Pi5 GPIO 29.
                // ResourceSource must name the GPIO controller; pin = 29.
                GpioIo (Exclusive, PullDefault, 0, 0, IoRestrictionOutputOnly,
                    "\\_SB.GPI0", 0, ResourceConsumer, , ) { 29 }
            })
            Return (RBUF)
        }

        // local-bd-address (Pi5 sample 88:a2:9e:58:5f:d6) is normally provided
        // by the platform; the driver can also read it from _DSM/registry.
    }
}
