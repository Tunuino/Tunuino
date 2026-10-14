# Firmware migration baseline

The target is source-level and build-level compatibility with the reference ECU
firmware architecture, adapted for Tunuino hardware.

## Source layout

The reference firmware currently contains these top-level directories:

`bin/`, `bootloader/`, `config/`, `console/`, `controllers/`,
`development/`, `docs/`, `ext/`, `ext_algo/`, `hw_layer/`,
`init/`, `integration/`, `live_data_generated/`, `pch/`,
`scripts/`, `tunerstudio/`, and `util/`.

The current Tunuino firmware instead contains `bootloader/`, `console/`,
`core/`, and `platform/`. This is a **migration**, not an assertion that
those architectures are already equivalent.

## Migration order and acceptance gates

1. Bring in the actual reference build system and board configuration structure,
   preserving required copyright notices and license files.
2. Port the original platform and hardware abstraction to the STM32F429 board.
3. Bring in engine control, sensors, trigger processing and scheduling using
   the reference module boundaries, without activating physical outputs.
4. Port firmware-side console transports, binary protocol and configuration
   generation together with their dependencies.
5. Port the corresponding user-facing console, bootloader and update tools.
6. Align CI jobs to real firmware, bootloader, console and unit-test builds.

Each step must compile and pass applicable tests before merging. Unsupported
hardware and missing dependencies must be explicitly reported, not stubbed as
working implementations.

## Attribution and naming

The application and project name are Tunuino. Original upstream copyright,
license and attribution notices must be retained where required. The origin of
imported code must remain traceable in the Git history and accompanying notices.

## Hardware safety

USB communication, flashing, injection and ignition must not be described as
working until tested on the target hardware. Never enable engine outputs or
flash writes merely to make a CI build pass.
