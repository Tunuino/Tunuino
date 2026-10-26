# Proteus v0.7 F427 ChibiOS USB implementation

The current image is the [0.2.0 input/calibration profile](proteus-io.md),
with ADC, missing-tooth inputs and persistent maps. No engine outputs are
initialized. The former identity-only behavior remains in regression fixtures.

The default `tools/build_proteus_usb_bench.py` entry point builds
`firmware/boards/proteus_v07/chibios_usb_bench` through
`tools/build_chibios_bench.py`. The earlier TinyUSB implementation is retained
for reference and regression tests; it is no longer the default image.

The dependency is the user's `Tunuino/ChibiOS`, fixed in `dependencies.json`.
It supplies the Cortex-M4 startup, STM32F427 clock initialization, ChibiOS/RT,
HAL USB OTG FS, and SerialUSB CDC. No changes are made to the dependency.
The configuration headers and USB descriptor/callback implementation are
adapted from its STM32F429 Discovery demo, retaining their notices.

## Hardware and USB configuration

- HSE 8 MHz, PLL M=8/N=336/P=2/Q=7: SYSCLK 168 MHz and USB 48 MHz.
- `STM32F427xx`, USBD1/OTG FS, PA11 and PA12 AF10.
- `BOARD_OTG_NOVBUSSENS` enables the internal PHY without PA9 VBUS sensing.
- Input acquisition and configuration as specified in `proteus-io.md`;
  no actuator GPIO or engine-control initialization.
- Flash starts at 0x08000000, replacing any installed flash bootloader.
  ROM DFU remains separate. BOOT0 must be returned to normal before reset.
- The linker restricts allocated flash to 128 KiB and SRAM to a 64 KiB subset.
- USB device CDC class 0x02; communications/data interfaces; interrupt IN EP1,
  bulk OUT/IN EP2. VID/PID CAFE:4001 are development identifiers.
- Product: `Proteus v0.7 ChibiOS USB bench`. Serial: 24 hexadecimal digits from
  the real STM32 UID. Tunuino identity firmware version 0.2.0.

The startup sequence 
`usb_serial_start()` pattern:
`halInit`, `chSysInit`, `sduObjectInit`, `sduStart`, USB disconnect, 250 ms sleep,
`usbStart`, USB connect, capture resynchronization. Both the native Console
protocol and the matching TunerStudio IO profile use the same dispatcher.

## Transport and callback context

The main thread owns the portable protocol link. Reads preserve any second
request coalesced with the first. Writes are limited to short USB packets
(63 bytes). Queue acceptance never advances the link: completion requires
`CHN_TRANSMISSION_END` from ChibiOS `sduDataTransmitted`.

USB callbacks use the ChibiOS ISR lock functions for I-class driver hooks.
A generation counter changes on configure/reset/unconfigure/suspend. If a
session has received protocol data, such a change stops the USB controller
before aborting the link. CRC errors and protocol deadlines do the same.
The controller and CDC queues restart after a 250 ms detach; the client must
reopen its session. Acquisition is resynchronized. The CDC driver does not
use DTR changes to define a protocol session.

## Build and distribution

With the dependency checked out at the locked commit in `.deps/chibios` and
the ARM toolchain on PATH:

```
python3 tools/build_proteus_usb_bench.py --output build/proteus-v07-usb-bench
```

The builder checks linked reset, SysTick and USB vectors; required ChibiOS
symbols; image bounds and initial stack; undefined symbols; and USB
descriptors extracted from the final flash image. The manifest records the
backend, clocks, dependency, source commit, image hashes and diagnostic symbols.

The linked firmware includes GPLv3 ChibiOS/RT. Its corresponding source archive
and license notices travel inside `firmware/` in the one combined Release ZIP.
Tunuino's own source retains its MIT notices; adapted HAL/config files retain
Apache-2.0 notices. The archive can be built directly with the included Makefile.

Host tests compile the actual main loop against a deterministic driver model:
startup order/detach delay, two coalesced requests, delayed physical completion,
CRC failure, TX timeout, disconnect and reconfiguration with a stale completion.
These tests do not emulate the STM32 USB peripheral or demonstrate physical
USB enumeration. Hardware results and user instructions belong in
[Tunuino_documents](https://github.com/Tunuino/Tunuino_documents).
