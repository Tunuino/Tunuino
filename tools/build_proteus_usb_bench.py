#!/usr/bin/env python3
"""Build experimental Proteus v0.7 / STM32F427 USB-only bench firmware."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BOARD = ROOT / "firmware/boards/proteus_v07/usb_bench"


def run(args):
    try:
        return subprocess.run(args, cwd=ROOT, check=True, text=True,
                              capture_output=True, timeout=120).stdout.strip()
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.stderr or str(error)) from error


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dependencies", type=Path, default=ROOT / ".deps")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    lock = json.loads((BOARD / "dependencies.json").read_text())
    dependencies = args.dependencies.resolve()
    for name, dependency in lock.items():
        path = dependencies / name
        if run(["git", "-C", str(path), "rev-parse", "HEAD"]) != dependency["commit"]:
            raise SystemExit(f"Wrong dependency commit: {name}")
        if not (path / dependency["license"]).is_file():
            raise SystemExit(f"Missing third-party license: {name}")
    tools = ["gcc", "objcopy", "nm", "size", "readelf"]
    for tool in tools:
        if not shutil.which("arm-none-eabi-" + tool):
            raise SystemExit(f"Required tool not found: arm-none-eabi-{tool}")
    if run(["arm-none-eabi-gcc", "-dumpmachine"]) != "arm-none-eabi":
        raise SystemExit("ARM bare-metal compiler required")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    objects = output / "objects"
    objects.mkdir()
    usb = dependencies / "tinyusb"
    device = dependencies / "cmsis_device"
    core = dependencies / "cmsis_core"
    flags = ["-mcpu=cortex-m4", "-mthumb", "-mfloat-abi=soft", "-Os", "-g3",
             "-ffunction-sections", "-fdata-sections", "-fno-common", "-fstack-usage",
             "-DSTM32F427xx", "-DHSE_VALUE=8000000u"]
    includes = [BOARD, ROOT / "firmware/include", usb / "src", device / "Include",
                core / "CMSIS/Core/Include"]
    flags += ["-I" + str(p) for p in includes]
    own = sorted(BOARD.glob("*.c")) + sorted((ROOT / "firmware/src").glob("*.c"))
    vendor = [usb / "src" / p for p in [
        "tusb.c", "common/tusb_fifo.c", "device/usbd.c", "device/usbd_control.c",
        "class/cdc/cdc_device.c", "portable/synopsys/dwc2/dcd_dwc2.c",
        "portable/synopsys/dwc2/dwc2_common.c"]]
    startup = device / "Source/Templates/gcc/startup_stm32f427xx.s"
    compiled = []
    for number, source in enumerate(own + vendor + [startup]):
        obj = objects / f"{number:02d}-{source.stem}.o"
        warnings = [] if source == startup else ["-std=gnu11", "-Wall", "-Wextra"]
        if source in own:
            warnings += ["-Werror"]
        run(["arm-none-eabi-gcc", *flags, *warnings, "-c", str(source), "-o", str(obj)])
        compiled.append(obj)
    atomic_symbols = run(["arm-none-eabi-nm", "--undefined-only", *map(str, compiled)])
    if "__atomic_" in atomic_symbols or "__sync_" in atomic_symbols:
        raise SystemExit("Unexpected atomic helper dependency")
    elf = output / "tunuino-proteus-v07-f427-usb-bench.elf"
    binary = elf.with_suffix(".bin")
    hexfile = elf.with_suffix(".hex")
    run(["arm-none-eabi-gcc", *flags, "-nostartfiles", "--specs=nosys.specs",
         "-T" + str(BOARD / "stm32f427_bench.ld"), "-Wl,--gc-sections",
         "-Wl,-Map=" + str(output / "firmware.map"), *map(str, compiled),
         "-Wl,--start-group", "-lc", "-lm", "-lgcc", "-lnosys", "-Wl,--end-group",
         "-o", str(elf)])
    if run(["arm-none-eabi-nm", "--undefined-only", str(elf)]):
        raise SystemExit("Unresolved symbols in final firmware")
    run(["arm-none-eabi-objcopy", "-O", "binary", str(elf), str(binary)])
    run(["arm-none-eabi-objcopy", "-O", "ihex", str(elf), str(hexfile)])
    image = binary.read_bytes()
    if not 8 <= len(image) <= 128 * 1024:
        raise SystemExit("Invalid flash image size")
    stack, reset = struct.unpack_from("<II", image)
    if stack != 0x20010000 or not reset & 1 or not 0x08000000 <= (reset & ~1) < 0x08000000 + len(image):
        raise SystemExit("Invalid startup vectors")
    (output / "elf-layout.txt").write_text(run(["arm-none-eabi-readelf", "-h", "-l", "-A", str(elf)]) + "\n")
    (output / "size.txt").write_text(run(["arm-none-eabi-size", str(elf)]) + "\n")
    notices = output / "third-party-licenses"
    notices.mkdir()
    for name, dependency in lock.items():
        shutil.copyfile(dependencies / name / dependency["license"], notices / (name + "-LICENSE.txt"))
    shutil.copyfile(ROOT / "LICENSE", output / "LICENSE.txt")
    manifest = {
        "target": "Proteus v0.7 STM32F427 USB-only bench",
        "status": "experimental_not_hardware_validated",
        "source_commit": os.environ.get("GITHUB_SHA"),
        "compiler": run(["arm-none-eabi-gcc", "--version"]).splitlines()[0],
        "dependencies": lock, "hse_hz": 8000000, "system_hz": 48000000,
        "flash_address": "0x08000000", "flash_limit_bytes": 128 * 1024,
        "ram_address": "0x20000000", "ram_limit_bytes": 64 * 1024,
        "initial_stack": hex(stack), "reset_vector": hex(reset),
        "capabilities": ["USB CDC", "real STM32 UID", "Tunuino identity protocol"],
        "engine_outputs": False, "physical_sensors": False, "maps": False,
        "usb_vid": "0xcafe", "usb_pid": "0x4001", "usb_ids": "example/development_only_not_production_allocation",
        "artifacts_sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in (elf,binary,hexfile)},
        "own_inputs_sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                              for p in own + sorted(BOARD.glob("*.h")) + [BOARD / "stm32f427_bench.ld"]},
        "limitations": ["No physical test has been performed",
                        "Base address replaces the installed application/flash bootloader",
                        "Backup and disconnected actuators required before first bench flash",
                        "Reset/replug USB after protocol fault or closing the serial session"],
    }
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print((output / "size.txt").read_text().strip())
    print(f"Linked bench image, {len(image)} flash bytes. Physical validation still required.")


if __name__ == "__main__":
    main()
