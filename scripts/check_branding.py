#!/usr/bin/env python3
"""Check tracked Tunuino-owned text files for legacy project branding."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
# Construct the forbidden name so this checker does not flag itself.
FORBIDDEN = b"ruse" + b"fi"
paths = subprocess.check_output(["git", "-C", str(ROOT), "ls-files", "-z"]).split(b"\0")
violations = []
checked = 0
for raw in paths:
    if not raw:
        continue
    relative = raw.decode("utf-8", "surrogateescape")
    path = ROOT / relative
    if not path.is_file() or path.is_symlink():
        continue
    content = path.read_bytes()
    if b"\0" in content[:8192]:
        continue
    checked += 1
    for number, line in enumerate(content.splitlines(), 1):
        if FORBIDDEN in line.lower():
            violations.append(f"{relative}:{number}")
print(f"Checked {checked} tracked text files.")
if violations:
    print("Legacy branding found:", *violations, sep="\n")
    sys.exit(1)
print("No legacy branding found in tracked text files.")
