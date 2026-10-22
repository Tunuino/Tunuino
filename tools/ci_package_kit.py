#!/usr/bin/env python3
"""Validate that downloaded bench components belong to this exact CI commit."""
import hashlib
import json
import os
from pathlib import Path


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
    manifest = {"source_commit": commit, "run_id": os.environ["GITHUB_RUN_ID"],
                "firmware": firmware, "console": console,
                "physical_ecu_validation": False}
    (root / "kit-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    (root / "START-HERE.txt").write_text(
        "Experimental Proteus v0.7 / F427 USB bench kit. Not an engine-control image.\n"
        "Read https://github.com/Tunuino/Tunuino_documents/blob/main/proteus-v07-bringup.md\n"
        "Backup the installed firmware before flashing; image starts at 0x08000000.\n"
        "Disconnect engine actuators for the bench test. Physical USB validation is pending.\n"
        "Extract the whole Console directory, then run console/TunuinoConsole.exe.\n")
    print(f"Validated synchronized bench kit for {commit}")


if __name__ == "__main__":
    main()
