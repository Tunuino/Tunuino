#!/usr/bin/env python3
"""Verify that every declared submodule appears in the license inventory."""
import configparser
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def main():
    cfg = configparser.ConfigParser(interpolation=None)
    if not cfg.read(ROOT / ".gitmodules"):
        raise SystemExit("Missing .gitmodules")
    inventory = (ROOT / "THIRD_PARTY_LICENSES.md").read_text(encoding="utf-8")
    expected = {}
    for section in cfg.sections():
        path = cfg.get(section, "path")
        url = cfg.get(section, "url")
        expected[path] = url
    actual = {}
    for line in inventory.splitlines():
        match = re.fullmatch(r"\| `([^`]+)` \| \[Upstream\]\((https?://[^)]+)\) \| Review pinned revision \|", line)
        if match:
            path, url = match.groups()
            if path in actual:
                raise SystemExit(f"Duplicate inventory entry: {path}")
            actual[path] = url
    if actual != expected:
        missing = sorted(expected.keys() - actual.keys())
        extra = sorted(actual.keys() - expected.keys())
        changed = sorted(p for p in expected.keys() & actual.keys() if expected[p] != actual[p])
        raise SystemExit(f"Third-party inventory mismatch: missing={missing}, extra={extra}, upstream_changed={changed}")
    print(f"Verified {len(expected)} third-party submodule inventory entries")


if __name__ == "__main__":
    main()
