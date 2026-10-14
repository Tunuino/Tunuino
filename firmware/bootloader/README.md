# Tunuino Bootloader

This directory is reserved for the Tunuino STM32F429 bootloader.

**Status:** no bootloader sources or binaries have been integrated. The CI
workflow validates the project layout and will compile sources only after a
real Makefile and implementation are provided.

Requirements before enabling firmware updates:

- Confirm flash sectors, application start address and vector-table alignment.
- Verify USB device transport, identifiers, command protocol and recovery path.
- Ensure the bootloader cannot write outside the intended application area.
- Verify firmware authenticity/integrity and power-loss behavior on hardware.
- Keep injector and ignition outputs disabled during programming.

Do not flash the offset-test image onto a board without a matching verified
bootloader. All firmware programming remains disabled in the Console.

The standalone `flash_layout.h` provides bounds and vector checks for future
bootloader code. It does not erase/program Flash, enable USB, or establish an
application offset. The header is not a functional bootloader.
