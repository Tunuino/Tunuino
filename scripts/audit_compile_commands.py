#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
"""Audit the source files represented in a CMake compilation database."""

import argparse
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("compilation_database", type=Path)
    parser.add_argument("--repository", type=Path, default=Path("."))
    args = parser.parse_args()

    root = args.repository.resolve()
    with args.compilation_database.open(encoding="utf-8") as stream:
        commands = json.load(stream)
    if not isinstance(commands, list):
        parser.error("compilation database must be a JSON array")

    compiled = set()
    for entry in commands:
        source = Path(entry["file"])
        if not source.is_absolute():
            source = Path(entry["directory"]) / source
        source = source.resolve()
        try:
            compiled.add(source.relative_to(root).as_posix())
        except ValueError:
            continue

    groups = {
        "firmware": sorted(path.relative_to(root).as_posix()
                           for path in (root / "firmware").rglob("*.c")),
        "tests": sorted(path.relative_to(root).as_posix()
                        for path in (root / "tests").rglob("*.c")),
    }
    print(f"Compilation commands: {len(commands)}")
    print(f"Distinct repository sources compiled: {len(compiled)}")
    for group, paths in groups.items():
        missing = sorted(set(paths) - compiled)
        print(f"\n{group}: {len(paths) - len(missing)}/{len(paths)} compiled")
        if missing:
            print("Not compiled by this configuration:")
            for path in missing:
                print(f"  {path}")
    # Missing sources are diagnostic: platform-specific files may belong to
    # another build configuration and must not fail this native audit.


if __name__ == "__main__":
    main()
