#!/usr/bin/env python3
"""Report architectural directory coverage against the reference ECU layout.

A directory match is not a claim of functional equivalence. This check is
informational and never downloads or copies third-party source code.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REFERENCE_ROOT = (
    "android", "bin", "docs", "firmware", "hardware", "java_console",
    "java_tools", "misc", "simulator", "unit_tests",
)
REFERENCE_FIRMWARE = (
    "bin", "bootloader", "config", "console", "controllers", "development",
    "docs", "ext", "ext_algo", "hw_layer", "init", "integration",
    "live_data_generated", "pch", "scripts", "tunerstudio", "util",
)


def report(base: Path, expected: tuple[str, ...], label: str) -> int:
    present = sorted(name for name in expected if (base / name).is_dir())
    missing = sorted(set(expected) - set(present))
    print(f"{label}: {len(present)}/{len(expected)} reference directories present")
    print("  Present: " + (", ".join(present) or "none"))
    print("  Missing: " + (", ".join(missing) or "none"))
    return len(missing)


if __name__ == "__main__":
    print("Reference architecture snapshot; names only, not functional parity.")
    report(ROOT, REFERENCE_ROOT, "Repository root")
    report(ROOT / "firmware", REFERENCE_FIRMWARE, "Firmware")
