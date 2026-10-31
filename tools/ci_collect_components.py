#!/usr/bin/env python3
"""Collect only successful independent component builds for the same source commit."""
import json
import os
import subprocess
import time
from pathlib import Path

REQUIRED = {
    "build-unit-tests.yml": None,
    "build-firmware.yml": ("firmware-proteus-f427-", "build/kit/firmware"),
    "build-console.yml": ("console-windows-x64-", "build/kit/console"),
    "build-desktop.yml": None,
}


def gh(*args):
    result = subprocess.run(["gh", *args], capture_output=True, text=True, check=True, timeout=90)
    return result.stdout


def main():
    sha = os.environ["SOURCE_SHA"]
    repo = os.environ["GH_REPO"]
    if not sha or len(sha) != 40:
        raise SystemExit("Expected a complete source commit SHA")
    deadline = time.monotonic() + 18 * 60
    runs = {}
    while time.monotonic() < deadline:
        runs.clear()
        for workflow in REQUIRED:
            data = json.loads(gh("api", f"repos/{repo}/actions/workflows/{workflow}/runs",
                                 "-f", "per_page=40", "-f", "head_sha=" + sha,
                                 "-f", "event=push", "--method", "GET"))
            matching = [run for run in data["workflow_runs"]
                        if run["head_sha"] == sha and run["event"] == "push"
                        and run["head_branch"] == "master"]
            matching.sort(key=lambda run: run["run_number"], reverse=True)
            if matching:
                runs[workflow] = matching[0]
        if len(runs) == len(REQUIRED) and all(
                run["status"] == "completed" for run in runs.values()):
            break
        time.sleep(20)
    else:
        raise SystemExit("Timed out waiting for all independent workflows at " + sha)
    for workflow, run in runs.items():
        if run["conclusion"] != "success":
            raise SystemExit(f"{workflow} is {run['conclusion']} for {sha}; refusing Bundle")
        print(f"Verified {workflow}: run {run['id']} at {sha}", flush=True)
    for workflow, spec in REQUIRED.items():
        if spec is None:
            continue
        prefix, destination = spec
        run = runs[workflow]
        artifact_name = prefix + sha
        artifacts = json.loads(gh("api", f"repos/{repo}/actions/runs/{run['id']}/artifacts",
                                  "-f", "per_page=100", "--method", "GET"))
        if not any(a["name"] == artifact_name and not a["expired"]
                   for a in artifacts["artifacts"]):
            raise SystemExit(f"Missing artifact {artifact_name} from run {run['id']}")
        Path(destination).mkdir(parents=True, exist_ok=True)
        subprocess.run(["gh", "run", "download", str(run["id"]), "-R", repo,
                        "-n", artifact_name, "-D", destination], check=True, timeout=240)
    print("All components verified and downloaded from identical commit " + sha)


if __name__ == "__main__":
    main()
