# Tunuino STM32F429 firmware programming (initial implementation)

This command-line programmer is a first executable flashing path for the Console. It requires an ST-Link connected to the **Tunuino STM32F429** SWD pins and STM32CubeProgrammer CLI installed.

**This is not yet USB/DFU flashing and not a graphical Console control.** It does not automatically identify the target MCU. The operator must independently confirm the connected target is the intended STM32F429 board, disconnect injector/ignition loads and ensure stable power. Never flash an unknown ECU.

```sh
python3 firmware/console/flash_stm32.py firmware.bin --expected-sha256 YOUR_APPROVED_SHA256 --confirm --dry-run
python3 firmware/console/flash_stm32.py firmware.bin --expected-sha256 YOUR_APPROVED_SHA256 --expected-device-id 0x419 --confirm
```

Only `.bin` images with plausible Cortex-M initial stack pointer and reset vector are accepted. This **does not authenticate the firmware** or prove board compatibility. Programming uses STM32CubeProgrammer SWD write at `0x08000000` with `-v` verification. A successful verify does not establish engine safety or hardware validation.

DFU, OpenBLT, firmware signing and automatic target identification remain pending.

Physical programming probes the STM32 device ID before writing; the example `0x419` must be verified against the intended STM32F429 silicon and the CubeProgrammer output. This is not a full board identity check.

## Desktop firmware panel

Run `python3 firmware/console/firmware_panel.py` on a desktop with Python Tkinter and STM32CubeProgrammer installed. This initial graphical firmware panel selects a `.bin` image, displays its SHA-256, requests an independently approved SHA-256 and device ID, and invokes the SWD flashing tool only after two confirmations. It is **not** the complete full tuning Console, and does not yet support USB DFU, OpenBLT, or automatic board identity verification. Never program an ECU with injector/ignition loads connected.
