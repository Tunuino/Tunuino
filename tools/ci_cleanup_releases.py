#!/usr/bin/env python3
"""Remove superseded generated attachments, preserving every existing bundle."""
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

REPOSITORY = "Tunuino/Tunuino"
BUNDLE = "tunuino_bundle_proteus_v07_f427_usb_bench.zip"
OBSOLETE = frozenset({"tunuino_console_windows_x64.zip",
                      "tunuino_firmware_proteus_v07_f427_usb_bench.zip",
                      "kit-manifest.json", "START-HERE.txt", "SHA256SUMS.txt"})


def obsolete_assets(release):
    if release.get("draft") or not release.get("tag_name", "").startswith("nightly-"):
        return []
    assets = release.get("assets", [])
    if not any(a["name"] == BUNDLE and a.get("state") == "uploaded" and a.get("size", 0) > 0 for a in assets):
        return []
    return [a for a in assets if a["name"] in OBSOLETE]


def revised_notes(body):
    lines = [line for line in body.splitlines()
             if not (line.startswith("|") and any(f"`{name}`" in line for name in OBSOLETE))]
    notes = "\n".join(lines).replace(
        "`console/TunuinoConsole.exe` (kit) ou `TunuinoConsole/TunuinoConsole.exe` (Console separada)",
        "`console/TunuinoConsole.exe`") + "\n"
    # Older notes described USB initialization through an external ECU project.
    # Keep the actual configuration and present it under the Tunuino identity.
    return re.sub(r"Segue o ciclo USB da [^:\n]+: ", "Configuração USB: ", notes)


def main():
    if os.environ.get("GH_REPO") != REPOSITORY:
        raise SystemExit("Release cleanup is restricted to Tunuino/Tunuino")
    pages = json.loads(subprocess.check_output(
        ["gh", "api", "--paginate", "--slurp", f"repos/{REPOSITORY}/releases?per_page=100"], text=True))
    removed = 0
    updated = 0
    for release in (release for page in pages for release in page):
        if release.get("draft") or not release.get("tag_name", "").startswith("nightly-"):
            continue
        old = obsolete_assets(release)
        body = release.get("body") or ""
        notes = revised_notes(body)
        if not old and notes == body:
            continue
        for asset in old:
            subprocess.run(["gh", "api", "--method", "DELETE",
                            f"repos/{REPOSITORY}/releases/assets/{int(asset['id'])}"], check=True)
            removed += 1
            print(f"Removed {release['tag_name']}/{asset['name']}")
        with tempfile.TemporaryDirectory() as work:
            payload = Path(work) / "notes.json"
            payload.write_text(json.dumps({"body": notes}), encoding="utf-8")
            subprocess.run(["gh", "api", "--method", "PATCH",
                            f"repos/{REPOSITORY}/releases/{int(release['id'])}", "--input", str(payload)],
                           check=True, stdout=subprocess.DEVNULL)
            updated += 1
    print(f"Removed {removed} obsolete attachments; existing bundles preserved")
    print(f"Updated {updated} nightly release descriptions")


if __name__ == "__main__":
    main()
