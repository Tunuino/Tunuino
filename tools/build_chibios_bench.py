#!/usr/bin/env python3
"""Build and inspect the Proteus USB bench with the user's pinned ChibiOS."""
import argparse
import configparser
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[1]
BOARD = ROOT / "firmware/boards/proteus_v07/chibios_usb_bench"
NAME = "tunuino-proteus-v07-f427-usb-bench"


def checkout_revision(path):
    """Read checkout identity without invoking Git inside the dependency."""
    metadata = path / ".git"
    if metadata.is_file():
        metadata = (path / metadata.read_text().strip().split(": ", 1)[1]).resolve()
    head = (metadata / "HEAD").read_text().strip()
    if head.startswith("ref: "):
        ref = head[5:]
        if (metadata / ref).is_file():
            return (metadata / ref).read_text().strip()
        for line in (metadata / "packed-refs").read_text().splitlines():
            if line.endswith(" " + ref):
                return line.split()[0]
        raise ValueError("Unresolved dependency revision")
    return head


def run(args, **kwargs):
    result = subprocess.run(args, cwd=ROOT, text=True, capture_output=True, timeout=180, **kwargs)
    if result.returncode:
        raise SystemExit((result.stdout + result.stderr)[-12000:])
    return result.stdout.strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dependencies", type=Path, default=ROOT / ".deps")
    parser.add_argument("--chibios", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    dependency = (args.chibios or args.dependencies / "chibios").resolve()
    lock = json.loads((BOARD / "dependencies.json").read_text())
    if checkout_revision(dependency) != lock["chibios"]["commit"]:
        raise SystemExit("ChibiOS revision differs from dependencies.json")
    if not shutil.which("arm-none-eabi-gcc"):
        raise SystemExit("ARM toolchain missing; run tools/setup_arm_toolchain.sh")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    compiled = output / "compile"
    result = subprocess.run(["make", "--no-print-directory", "-C", str(BOARD),
                             "CHIBIOS=" + str(dependency), "BUILDDIR=" + str(compiled), "-j2"],
                            cwd=ROOT, text=True, capture_output=True, timeout=180)
    (output / "build.txt").write_text(result.stdout + result.stderr)
    if result.returncode:
        raise SystemExit((result.stdout + result.stderr)[-12000:])
    for suffix in (".elf", ".bin", ".hex"):
        shutil.copy2(compiled / (NAME + suffix), output / (NAME + suffix))
    shutil.copy2(compiled / (NAME + ".map"), output / "firmware.map")
    elf = output / (NAME + ".elf")
    binary = output / (NAME + ".bin")
    image = binary.read_bytes()
    stack, reset = struct.unpack_from("<II", image)
    if not 8 <= len(image) <= 128 * 1024 or not 0x20000000 < stack <= 0x20010000 or stack & 7:
        raise SystemExit("Invalid image size or initial stack")
    if not reset & 1 or not 0x08000000 <= (reset & ~1) < 0x08000000 + len(image):
        raise SystemExit("Reset vector is outside the image")
    if run(["arm-none-eabi-nm", "--undefined-only", str(elf)]):
        raise SystemExit("Undefined ELF symbols")
    symbols = {}
    for line in run(["arm-none-eabi-nm", "--defined-only", str(elf)]).splitlines():
        fields = line.split()
        if len(fields) == 3:
            symbols[fields[2]] = int(fields[0], 16)
    vectors = {"Reset_Handler": 1, "SysTick_Handler": 15, "Vector14C": 16 + 67}
    for name, index in vectors.items():
        value = struct.unpack_from("<I", image, index * 4)[0]
        if (value & ~1) != (symbols[name] & ~1) or not value & 1:
            raise SystemExit("Wrong hardware vector: " + name)
    for name in ("halInit", "chSysInit", "usbStart", "sduStart", "__early_init"):
        if name not in symbols:
            raise SystemExit("Missing ChibiOS startup/USB symbol: " + name)
    # Inspect descriptors from the linked flash image, not only their C source.
    def flash_data(name, length):
        offset = symbols[name] - 0x08000000
        if offset < 0 or offset + length > len(image):
            raise SystemExit("USB descriptor outside flash: " + name)
        return image[offset:offset + length]

    device = flash_data("vcom_device_descriptor_data", 18)
    if device != struct.pack("<BBHBBBBHHHBBBB", 18, 1, 0x0200, 2, 0, 0, 64,
                             0xCAFE, 0x4001, 2, 1, 2, 3, 1):
        raise SystemExit("Unexpected linked CDC device descriptor")
    configuration = flash_data("vcom_configuration_descriptor_data", 67)
    descriptors, offset = [], 0
    while offset < len(configuration):
        length = configuration[offset]
        if length < 2 or offset + length > len(configuration):
            raise SystemExit("Invalid USB descriptor length")
        descriptors.append(configuration[offset:offset + length])
        offset += length
    interfaces = [d[5] for d in descriptors if d[1] == 4]
    endpoints = [(d[2], d[3], int.from_bytes(d[4:6], "little")) for d in descriptors if d[1] == 5]
    if (int.from_bytes(configuration[2:4], "little") != 67 or interfaces != [2, 10]
            or endpoints != [(0x81, 3, 8), (0x02, 2, 64), (0x82, 2, 64)]):
        raise SystemExit("Unexpected linked CDC interfaces/endpoints")
    for name, length in (("vcom_string0", 4), ("vcom_string1", 16), ("vcom_string2", 62)):
        if flash_data(name, length)[:2] != bytes((length, 3)):
            raise SystemExit("Invalid USB string descriptor: " + name)
    (output / "elf-layout.txt").write_text(run(["arm-none-eabi-readelf", "-h", "-l", "-A", str(elf)]) + "\n")
    (output / "size.txt").write_text(run(["arm-none-eabi-size", str(elf)]) + "\n")
    notices = output / "third-party-licenses"
    notices.mkdir()
    shutil.copy2(dependency / "license.txt", notices / "ChibiOS-GPL-3.0.txt")
    shutil.copy2(BOARD / "LICENSES/Apache-2.0.txt", notices / "ChibiOS-HAL-Apache-2.0.txt")
    shutil.copy2(ROOT / "LICENSE", output / "LICENSE.txt")
    # GPL ChibiOS/RT is linked into the image. Include complete corresponding
    # source, configuration and build scripts in the same distributed bundle.
    source = output / "corresponding-source.tar.gz"
    with tarfile.open(source, "w:gz") as archive:
        for path in (ROOT / "firmware/src", ROOT / "firmware/include", ROOT / "firmware/tunerstudio", BOARD):
            archive.add(path, arcname=str(path.relative_to(ROOT)))
        for name in ("LICENSE", "tools/build_proteus_usb_bench.py", "tools/build_chibios_bench.py", "tools/setup_arm_toolchain.sh"):
            archive.add(ROOT / name, arcname=name)
        archive.add(dependency / "os", arcname=".deps/chibios/os")
        archive.add(dependency / "license.txt", arcname=".deps/chibios/license.txt")
    commit = os.environ.get("GITHUB_SHA", "development")
    flashing = binary.with_name(binary.name + ".properties")
    flashing.write_text("format=1\ntarget=proteus-v07-f427\nmode=dfu\naddress=0x08000000\n"
                        "replaces_bootloader=true\nsource_commit=" + commit + "\n"
                        "description=Proteus v0.7 STM32F427 - ChibiOS USB bench\n"
                        "sha256=" + hashlib.sha256(image).hexdigest() + "\n")
    ini = output / "tunuino_proteus_v07_f427_usb_bench.ini"
    shutil.copy2(ROOT / "firmware/tunerstudio" / ini.name, ini)
    definition = configparser.ConfigParser(interpolation=None, strict=False)
    definition.read_string("[General]\n" + ini.read_text(encoding="ascii"))
    signature = json.loads(definition["TunerStudio"]["signature"])
    if signature.encode("ascii") + b"\0" not in image:
        raise SystemExit("TunerStudio INI signature is not in the linked firmware")
    hashed = [elf, binary, elf.with_suffix(".hex"), flashing, source, ini]
    manifest = {"target": "Proteus v0.7 STM32F427 ChibiOS USB-only bench",
                "status": "experimental_not_hardware_validated", "source_commit": commit,
                "compiler": run(["arm-none-eabi-gcc", "--version"]).splitlines()[0],
                "dependencies": lock, "usb_backend": "ChibiOS HAL USB/SerialUSB",
                "firmware_license": "GPL-3.0-only (linked ChibiOS/RT); component notices included",
                "corresponding_source": source.name, "hse_hz": 8000000, "system_hz": 168000000,
                "flash_address": "0x08000000", "flash_limit_bytes": 128 * 1024,
                "ram_address": "0x20000000", "ram_limit_bytes": 64 * 1024,
                "initial_stack": hex(stack), "reset_vector": hex(reset),
                "capabilities": ["USB CDC", "real STM32 UID", "Tunuino identity protocol", "TunerStudio read-only identity"],
                "tunerstudio": {"ini_file": ini.name, "signature": signature,
                                "output_bytes": 31, "read_only": True},
                "engine_outputs": False, "physical_sensors": False, "maps": False,
                "usb_vid": "0xcafe", "usb_pid": "0x4001", "usb_disconnect_ms": 250,
                "usb_ids": "development_only_not_production_allocation",
                "artifacts_sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in hashed},
                "diagnostic_symbols": {n: hex(symbols[n]) for n in ("tunuino_bench_fault", "main", "__early_init")},
                "physical_ecu_validation": False}
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    (output / "SOURCE.txt").write_text(
        "Linked firmware: GPL-3.0-only because it includes ChibiOS/RT.\n"
        "Own Tunuino source files retain their MIT notices; HAL/startup configurations retain Apache-2.0 notices.\n"
        "Complete corresponding source is in corresponding-source.tar.gz, including ChibiOS os/ and build scripts.\n"
        "Extract it, install the toolchain with tools/setup_arm_toolchain.sh, then invoke make in\n"
        "firmware/boards/proteus_v07/chibios_usb_bench with CHIBIOS pointing to .deps/chibios.\n"
        "The build wrapper requires a pinned Git checkout; the source archive can be built directly with make.\n")
    print((output / "size.txt").read_text().strip())
    print(f"ChibiOS F427 image: {len(image)} flash bytes; physical USB validation pending")


if __name__ == "__main__":
    main()
