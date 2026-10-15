#!/usr/bin/env python3
"""Validate repository submodule declarations without fetching dependencies."""
import configparser
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]


def check():
    config = configparser.ConfigParser(interpolation=None)
    if not config.read(ROOT / ".gitmodules"):
        raise ValueError("Missing .gitmodules")

    declared = {}
    for section in config.sections():
        if not section.startswith('submodule '):
            raise ValueError(f"Unexpected .gitmodules section: {section}")
        path = config.get(section, "path", fallback="").strip()
        url = config.get(section, "url", fallback="").strip()
        if not path or not url:
            raise ValueError(f"Missing submodule path or URL in {section}")
        if pathlib.PurePosixPath(path).is_absolute() or ".." in pathlib.PurePosixPath(path).parts:
            raise ValueError(f"Unsafe submodule path: {path}")
        if path in declared:
            raise ValueError(f"Duplicate submodule path: {path}")
        declared[path] = url

    result = subprocess.run(
        ["git", "ls-files", "--stage", "-z"], cwd=ROOT, check=True,
        capture_output=True,
    )
    tracked = set()
    for entry in result.stdout.split(b"\0"):
        if not entry:
            continue
        metadata, filename = entry.split(b"\t", 1)
        if metadata.split(b" ", 1)[0] == b"160000":
            tracked.add(filename.decode("utf-8"))

    missing = sorted(tracked - declared.keys())
    extra = sorted(declared.keys() - tracked)
    if missing or extra:
        raise ValueError(f"Submodule mismatch: undeclared gitlinks={missing}, untracked declarations={extra}")
    print(f"Validated {len(tracked)} declared submodules and gitlinks")


if __name__ == "__main__":
    try:
        check()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print(f"Repository structure check failed: {error}", file=sys.stderr)
        sys.exit(1)
