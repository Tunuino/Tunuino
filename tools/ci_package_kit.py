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
    profiles={"tunuino_proteus_v07_f427_usb_bench.ini":("bench_tunerstudio.h","TUNUINO_TS_SIGNATURE",31,True),
              "tunuino_proteus_v07_f427_io.ini":("device.h","TUNUINO_IO_SIGNATURE",100,False)}
    if name not in profiles:raise ValueError("Unexpected TunerStudio definition")
    header_name,macro,output_bytes,read_only=profiles[name]
    if descriptor["read_only"] is not read_only:raise ValueError("Wrong TunerStudio access mode")
    path = root / "firmware" / name
    if hashlib.sha256(path.read_bytes()).hexdigest() != firmware["artifacts_sha256"][name]:
        raise ValueError("TunerStudio INI checksum mismatch")
    definition = configparser.ConfigParser(interpolation=None, strict=False)
    definition.read_string("[General]\n" + path.read_text(encoding="ascii"))
    header = (Path(__file__).resolve().parents[1] / "firmware/include/tunuino" / header_name).read_text()
    signature = re.search(rf'^#define {macro} "([^"]+)"$', header, re.M).group(1)
    if (json.loads(definition["TunerStudio"]["signature"]) != signature
            or descriptor["signature"] != signature or descriptor["output_bytes"] != output_bytes
            or definition["OutputChannels"].getint("ochBlockSize") != output_bytes):
        raise ValueError("TunerStudio definition does not match this firmware layout")
    return descriptor


def main():
    root = Path("build/kit")
    commit = os.environ.get("SOURCE_SHA", os.environ["GITHUB_SHA"])
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
    manifest = {"source_commit": commit, "run_id": os.environ.get("KIT_RUN_ID", os.environ["GITHUB_RUN_ID"]),
                "firmware": firmware, "console": console, "programmers": programmers,
                "tunerstudio": tunerstudio,
                "physical_ecu_validation": False}
    (root / "kit-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    (root / "START-HERE.txt").write_text(
        "Proteus v0.7 / F427 kit with persistent engine configuration and optional autonomous startup.\n"
        "Read https://github.com/Tunuino/Tunuino_documents/blob/main/proteus-v07-bringup.md\n"
        "Backup the installed firmware before flashing; image starts at 0x08000000.\n"
        "New engine-control behavior still requires physical timing and electrical validation.\n"
        "Extract the whole Console directory, then run console/TunuinoConsole.exe.\n"
        "Select firmware/*.bin in the Console; keep its .bin.properties beside it.\n"
        "DFU uses STM32_Programmer_CLI.exe from installed CubeProgrammer or an existing compatible bundle.\n"
        "See console/tools/PROGRAMMERS.txt for offline tool and driver configuration.\n"
        "Firmware uses Tunuino/ChibiOS; source and notices are included in firmware/.\n"
        "After verified flashing, restore normal BOOT0 and reset the ECU.\n"
        "Expected USB application: Proteus v0.7 ChibiOS USB bench (CDC COM).\n"
        "TunerStudio: close the Console COM connection, create a project, choose Other/Browse,\n"
        "then select firmware/tunuino_proteus_v07_f427_io.ini and the ECU COM at 115200.\n"
        "Use Firmware Default protocol. This INI requires the firmware in this same Bundle.\n"
        "Tunuino 0.2.0 IO: native 16x16 only; back up previous calibration before flashing.\n"
        "Console: Sensors and inputs; Engine and outputs; separate Actuator tests for injectors, ignition and assigned relays.\n"
        "Engine profile is saved atomically with sensors/maps. Verify pin mapping, TDC, dwell and injector calibration.\n"
        "Autonomous mode runs without USB. Manual mode retains a 2-second host supervision timeout.\n"
        "TPS/idle/boost: separate page 4, reserved LS outputs, PWM tables or PI and overboost cut. All start disabled.\n"
        "Fuel features: IAT/voltage compensation, lambda target/PI, TPS acceleration, DFCO, RPM/launch cuts, analog ethanol, protections and cylinder trims.\n"
        "New features start disabled; initialize references in Console, then calibrate sensors and settings. Times are milliseconds.\n"
        "Save separate .tnaux backups; old calibration records migrate with the original bank preserved until commit.\n"
        "Engine start/warm-up tab: optional cranking fuel, afterstart decay in ms, coolant curve and fixed cranking timing.\n"
        "Base VE/ignition maps are local editable references, not a validated calibration. Console times use milliseconds.\n"
        "AV readings are MCU ADC voltages, not uncorrected external sensor voltages.\n"
        "Flash sectors 5/6 at 0x08020000/0x08040000 store calibration; firmware image stays below 0x08020000.\n")
    print(f"Validated synchronized bench kit for {commit}")


if __name__ == "__main__":
    main()
