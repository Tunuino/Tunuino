#!/usr/bin/env python3
"""Cross-compile the portable core, NOT a bootable or flashable ECU image."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def run(args):
    try:
        return subprocess.run(args, cwd=ROOT, check=True, text=True,
                              capture_output=True, timeout=120).stdout.strip()
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.stderr or str(error)) from error


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpu", required=True, choices=["cortex-m4", "cortex-m7"])
    parser.add_argument("--output", required=True, type=Path,
                        help="New output directory; existing paths are never overwritten")
    args = parser.parse_args()
    compiler = "arm-none-eabi-gcc"
    archiver = "arm-none-eabi-ar"
    symbols = "arm-none-eabi-nm"
    for tool in (compiler, archiver, symbols, "arm-none-eabi-readelf"):
        if not shutil.which(tool):
            raise SystemExit(f"Required tool not found: {tool}")
    if run([compiler, "-dumpmachine"]) != "arm-none-eabi":
        raise SystemExit("An ARM bare-metal toolchain is required")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    objects = output / "objects"
    objects.mkdir()
    flags = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic", "-Os",
             f"-mcpu={args.cpu}", "-mthumb", "-mfloat-abi=soft", "-ffreestanding",
             "-ffunction-sections", "-fdata-sections", "-fno-common", "-Ifirmware/include"]
    sources = sorted((ROOT / "firmware/src").glob("*.c"))
    if not sources:
        raise SystemExit("No core sources found")
    compiled = []
    for source in sources:
        obj = objects / (source.stem + ".o")
        run([compiler, *flags, "-c", str(source), "-o", str(obj)])
        compiled.append(obj)
    # Lock-based atomic library calls would violate the acquisition contract.
    unresolved = run([symbols, "--undefined-only", *map(str, compiled)])
    if any("__atomic_" in line or "__sync_" in line for line in unresolved.splitlines()):
        raise SystemExit("Unexpected out-of-line atomic helper; inspect target support")
    library = output / f"libtunuino-core-{args.cpu}.a"
    run([archiver, "rcsD", str(library), *map(str, compiled)])
    attributes = run(["arm-none-eabi-readelf", "-A", str(compiled[0])])
    (output / "arm-attributes.txt").write_text(attributes + "\n", encoding="utf-8")
    (output / "undefined-symbols.txt").write_text(unresolved + "\n", encoding="utf-8")
    inputs = sources + sorted((ROOT / "firmware/include/tunuino").glob("*.h"))
    inputs.append(Path(__file__).resolve())
    manifest = {
        "artifact_type": "static_core_library_not_flashable",
        "cpu": args.cpu,
        "float_abi": "soft",
        "board": None,
        "mcu_part": None,
        "flashable": False,
        "source_commit": os.environ.get("GITHUB_SHA"),
        "compiler": run([compiler, "--version"]).splitlines()[0],
        "flags": flags,
        "source_count": len(sources),
        "inputs_sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in inputs},
        "library_sha256": hashlib.sha256(library.read_bytes()).hexdigest(),
        "limitations": ["No startup/vector table or linker memory layout",
                        "No board initialization, USB or sensor drivers",
                        "Not linked: libc, libm and compiler-runtime symbols remain unresolved",
                        "Compilation is not execution or physical ECU validation"],
    }
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Compiled {len(sources)} core sources for {args.cpu}: {library.name}")
    print("Static library only. NOT a firmware image; do not flash it to an ECU.")


if __name__ == "__main__":
    main()
