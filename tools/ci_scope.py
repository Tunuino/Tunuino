#!/usr/bin/env python3
"""Select affected CI components; manual and scheduled runs always produce a complete bench kit."""
import json
import os
from pathlib import Path
import subprocess


def scope(paths):
    all_components = any(p.startswith(".github/") or p.startswith("tools/ci_")
                         or p == "LICENSE" for p in paths)
    core = all_components or any(p.startswith(("firmware/", "console/", "tools/", "hardware/", "samples/")) for p in paths)
    arm = all_components or any(p.startswith(("firmware/src/", "firmware/include/"))
                               or p in ("tools/build_arm_core.py", "tools/setup_arm_toolchain.sh") for p in paths)
    bench = arm or any(p.startswith("firmware/boards/") or p == "tools/build_proteus_usb_bench.py" for p in paths)
    console = all_components or any(p.startswith("console/") and not p.startswith("console/src/test/") for p in paths)
    return {"core": core, "arm": arm, "bench": bench, "console": console}


def main():
    event = json.loads(Path(os.environ["GITHUB_EVENT_PATH"]).read_text())
    name = os.environ["GITHUB_EVENT_NAME"]
    try:
        if name in ("workflow_dispatch", "schedule"):
            result = {key: True for key in ("core", "arm", "bench", "console")}
        else:
            if name == "pull_request":
                base, head = event["pull_request"]["base"]["sha"], event["pull_request"]["head"]["sha"]
                base = subprocess.check_output(["git", "merge-base", base, head], text=True).strip()
            else:
                base, head = event["before"], event["after"]
                if not base.strip("0"):
                    raise ValueError("New history")
            changed = subprocess.check_output(["git", "diff", "--name-only", "-z", base, head])
            result = scope(changed.decode("utf-8", "surrogateescape").rstrip("\0").split("\0"))
    except (KeyError, ValueError, subprocess.CalledProcessError):
        # Unknown history must expand validation, never silently omit a build.
        result = {key: True for key in ("core", "arm", "bench", "console")}
    with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as output:
        for key, value in result.items():
            output.write(f"{key}={str(value).lower()}\n")
    print(json.dumps(result, sort_keys=True))


if __name__ == "__main__":
    main()
