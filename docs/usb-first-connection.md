# Tunuino STM32F429 USB driver and first connection

The ECU firmware USB peripheral is not implemented yet. This document defines the expected first-connection behavior; it does not claim hardware validation.

## Normal console connection (USB CDC ACM)

The ECU should enumerate with a Tunuino product string and a correctly assigned USB VID/PID, and expose a USB CDC ACM serial interface. A VID/PID must be legally assigned or licensed before release; do not reuse STMicroelectronics' or another vendor's identifiers without authorization.

- Windows 10/11: use the inbox USB CDC ACM driver (`usbser.sys`) when the device descriptors are compatible. Windows normally installs it automatically; an installation prompt is not guaranteed and cannot be forced by the Console.
- Linux: use the kernel `cdc_acm` driver (typically `/dev/ttyACM*`). Access may require user/group permissions.
- macOS: use the built-in CDC ACM driver (typically `/dev/cu.usbmodem*`).

The Console should list serial ports, identify the ECU with the read-only `S` signature query, and show clear troubleshooting if the port is missing or unavailable. Never claim a Tunuino connection based only on a USB product name or port label; confirm the returned ECU signature against the selected INI.

## Firmware programming mode

The STM32 system-memory DFU bootloader is a separate USB device mode with different descriptors and driver behavior. On Windows, STM32CubeProgrammer may require a suitable USB driver for DFU mode. Only offer a firmware programming action after explicit board identification and compatibility checks; never install arbitrary unsigned drivers.

## Implementation checklist

1. Implement and validate the STM32F429 USB FS hardware configuration and USB CDC device descriptors.
2. Allocate authorized USB VID/PID values and product/manufacturer strings.
3. Bridge USB CDC RX/TX to the read-only Tunuino protocol without interfering with the existing UART transport.
4. Add port discovery and driver-help messages to the desktop Console.
5. Validate enumeration on Windows, Linux and macOS using a real board.
6. Keep USB CDC diagnostics and DFU firmware programming as separate, explicitly labeled modes.
