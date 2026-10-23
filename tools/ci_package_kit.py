#!/usr/bin/env python3
"""Validate that downloaded bench components belong to this exact CI commit."""
import hashlib
import json
import os
from pathlib import Path
import configparser
import re


def validate_tunerstudio(root, firmware):
    descriptor = firmware["tunerstudio"]
    name = descriptor["ini_file"]
    if name != "tunuino_proteus_v07_f427_usb_bench.ini" or descriptor["read_only"] is not True:
        raise ValueError("Unexpected TunerStudio bench definition")
    path = root / "firmware" / name
    if hashlib.sha256(path.read_bytes()).hexdigest() != firmware["artifacts_sha256"][name]:
        raise ValueError("TunerStudio INI checksum mismatch")
    definition = configparser.ConfigParser(interpolation=None, strict=False)
    definition.read_string("[General]\n" + path.read_text(encoding="ascii"))
    header = (Path(__file__).resolve().parents[1] / "firmware/include/tunuino/bench_tunerstudio.h").read_text()
    signature = re.search(r'^#define TUNUINO_TS_SIGNATURE "([^"]+)"$', header, re.M).group(1)
    if (json.loads(definition["TunerStudio"]["signature"]) != signature
            or descriptor["signature"] != signature or descriptor["output_bytes"] != 31
            or definition["OutputChannels"].getint("ochBlockSize") != 31):
        raise ValueError("TunerStudio definition does not match this firmware layout")
    return descriptor


def main():
    root = Path("build/kit")
    commit = os.environ["GITHUB_SHA"]
    firmware = json.loads((root / "firmware/manifest.json").read_text())
    console = json.loads((root / "console/package-manifest.json").read_text())
    if firmware["source_commit"] != commit or console["source_commit"] != commit:
        raise SystemExit("Refusing a kit with mixed source commits")
    for name, digest in firmware["artifacts_sha256"].items():
        if Path(name).name != name or hashlib.sha256((root / "firmware" / name).read_bytes()).hexdigest() != digest:
            raise SystemExit("Firmware artifact checksum mismatch")
    jar = root / "console/app/tunuino-console.jar"
    if hashlib.sha256(jar.read_bytes()).hexdigest() != console["main_jar_sha256"]:
        raise SystemExit("Console artifact checksum mismatch")
    tunerstudio = validate_tunerstudio(root, firmware)
    programmers = json.loads((root / "console/tools/tools-manifest.json").read_text())
    for folder, entries in programmers["bundled"].items():
        if folder not in ("drivers", "STM32_Programmer_CLI", "openblt"):
            raise SystemExit("Unknown programmer distribution")
        base = root / "console" / ("drivers" if folder == "drivers" else "tools/" + folder)
        for name, digest in entries.items():
            path = base / name
            if not path.resolve().is_relative_to(base.resolve()) or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
                raise SystemExit("Programmer tool checksum mismatch")
    manifest = {"source_commit": commit, "run_id": os.environ["GITHUB_RUN_ID"],
                "firmware": firmware, "console": console, "programmers": programmers,
                "tunerstudio": tunerstudio,
                "physical_ecu_validation": False}
    (root / "kit-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    (root / "START-HERE.txt").write_text(
        "Experimental Proteus v0.7 / F427 ChibiOS USB bench kit. Not an engine-control image.\n"
        "Read https://github.com/Tunuino/Tunuino_documents/blob/main/proteus-v07-bringup.md\n"
        "Backup the installed firmware before flashing; image starts at 0x08000000.\n"
        "Disconnect engine actuators for the bench test. Physical USB validation is pending.\n"
        "Extract the whole Console directory, then run console/TunuinoConsole.exe.\n"
        "Select firmware/*.bin in the Console; keep its .bin.properties beside it.\n"
        "DFU uses STM32_Programmer_CLI.exe from installed CubeProgrammer or an existing compatible bundle.\n"
        "See console/tools/PROGRAMMERS.txt for offline tool and driver configuration.\n"
        "Firmware uses Tunuino/ChibiOS; source and notices are included in firmware/.\n"
        "After verified flashing, restore normal BOOT0 and reset the ECU.\n"
        "Expected USB application: Proteus v0.7 ChibiOS USB bench (CDC COM).\n"
        "TunerStudio: close the Console COM connection, create a project, choose Other/Browse,\n"
        "then select firmware/tunuino_proteus_v07_f427_usb_bench.ini and the ECU COM at 115200.\n"
        "Use Firmware Default protocol. This INI requires the firmware in this same Bundle.\n"
        "TunerStudio is read-only: identity/revision/version/UID, without engine sensors or maps.\n")
    print(f"Validated synchronized bench kit for {commit}")


if __name__ == "__main__":
    main()
