#!/usr/bin/env python3
"""Tunuino STM32 SWD firmware programmer. Requires STM32CubeProgrammer CLI."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys

FLASH_BASE = 0x08000000
MAX_IMAGE_BYTES = 2 * 1024 * 1024


def validate_image(path: Path) -> None:
    if path.suffix.lower() != ".bin":
        raise ValueError("Only raw .bin images are currently supported for SWD")
    if not path.is_file() or not 0 < path.stat().st_size <= MAX_IMAGE_BYTES:
        raise ValueError("Firmware image is missing, empty or too large")
    with path.open("rb") as stream:
        vector = stream.read(8)
    sp = int.from_bytes(vector[:4], "little")
    reset = int.from_bytes(vector[4:], "little")
    if not (0x20000000 <= sp < 0x20080000 and (reset & 1) and FLASH_BASE <= (reset & ~1) < FLASH_BASE + MAX_IMAGE_BYTES):
        raise ValueError("Invalid STM32 initial stack pointer or reset vector")


def flash(image: Path, programmer: str, confirmed: bool, dry_run: bool = False) -> list[str]:
    validate_image(image)
    if not confirmed:
        raise ValueError("Explicit --confirm required; disconnect ignition/injector loads")
    executable = shutil.which(programmer)
    if not executable and not dry_run:
        raise FileNotFoundError("STM32_Programmer_CLI not found")
    command = [executable or programmer, "-c", "port=SWD", "-w", str(image.resolve()), hex(FLASH_BASE), "-v"]
    if not dry_run:
        subprocess.run(command, check=True)
    return command


def main() -> int:
    parser = argparse.ArgumentParser(description="Program a verified Tunuino STM32F429 image over ST-Link SWD")
    parser.add_argument("image", type=Path)
    parser.add_argument("--programmer", default="STM32_Programmer_CLI")
    parser.add_argument("--confirm", action="store_true", help="Confirm correct STM32F429 board, power and disconnected loads")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    try:
        command = flash(args.image, args.programmer, args.confirm, args.dry_run)
        print(" ".join(command))
        print("Dry run only" if args.dry_run else "Programmer completed write and verify")
        return 0
    except (ValueError, FileNotFoundError, subprocess.CalledProcessError, OSError) as exc:
        print(f"Firmware programming refused or failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
