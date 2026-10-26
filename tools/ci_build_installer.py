#!/usr/bin/env python3
"""Package and exercise the Windows installer from an already validated kit."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

SETUP_NAME = "tunuino_setup_windows_x64.msi"
UPGRADE_UUID = "753927ab-8f0b-4d0c-b832-fbc38a77f21a"


def package_version(number):
    number = int(number)
    if not 0 <= number < 256 * 65536:
        raise ValueError("Installer run number exceeds MSI version limits")
    return f"1.{number // 65536}.{number % 65536}"


def prepare_image(kit, destination, commit):
    manifest = json.loads((kit / "kit-manifest.json").read_text(encoding="utf-8-sig"))
    if manifest["source_commit"] != commit:
        raise ValueError("Installer kit belongs to another commit")
    if not (kit / "console/app/.jpackage.xml").is_file():
        raise ValueError("Console artifact lost app/.jpackage.xml; upload it with include-hidden-files: true")
    shutil.copytree(kit / "console", destination)
    shutil.copytree(kit / "firmware", destination / "firmware")
    shutil.copy2(kit / "kit-manifest.json", destination / "kit-manifest.json")
    (destination / "INSTALLATION.txt").write_text(
        "Tunuino Console - Windows x64\n"
        "Open TunuinoConsole from the Start menu or desktop shortcut. Java is included.\n"
        "Firmware, matching TunerStudio INI, source and licenses are in the firmware folder beside the application.\n"
        "The firmware selector opens that folder, independently of the shortcut working directory.\n"
        "User files: %LOCALAPPDATA%\\TunuinoConsole\\maps, configurations and logs.\n"
        "Programmer settings from %USERPROFILE%\\.tunuino remain readable.\n"
        "Install a newer MSI to update. Close the Console before updating or uninstalling.\n"
        "Uninstall from Windows Settings > Apps. User files are retained.\n"
        "The installer does not flash the ECU or install third-party drivers.\n"
        "Experimental physical outputs: consult https://github.com/Tunuino/Tunuino_documents/blob/main/proteus-v07-bringup.md\n",
        encoding="utf-8")
    hashes = {}
    for path in sorted(p for p in destination.rglob("*") if p.is_file()):
        if path.is_symlink():
            raise ValueError("Symlink in installer payload")
        if path.name == ".jpackage.xml":
            continue  # jpackage's app-image metadata is not an installed application resource.
        hashes[path.relative_to(destination).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    (destination / "installation-manifest.json").write_text(json.dumps(
        {"source_commit": commit, "files_sha256": hashes}, indent=2) + "\n", encoding="utf-8")
    return manifest


def build_msi(image, destination, version):
    destination.mkdir(parents=True, exist_ok=True)
    command = ["jpackage", "--verbose", "--type", "msi", "--name", "TunuinoConsole",
               "--app-image", str(image.resolve()), "--app-version", version,
               "--vendor", "Tunuino", "--description", "Tunuino Console, firmware e INI TunerStudio",
               # A single directory avoids jpackage's ICE64 error for intermediate per-user directories.
               # Keep it distinct from %LOCALAPPDATA%/TunuinoConsole, which belongs to the user.
               "--dest", str(destination.resolve()), "--install-dir", "TunuinoConsoleApp",
               "--win-per-user-install", "--win-dir-chooser", "--win-menu",
               "--win-menu-group", "Tunuino", "--win-shortcut",
               "--win-upgrade-uuid", UPGRADE_UUID,
               "--win-help-url", "https://github.com/Tunuino/Tunuino_documents",
               "--win-update-url", "https://github.com/Tunuino/Tunuino/releases"]
    subprocess.run(command, check=True, timeout=240)
    packages = list(destination.glob("*.msi"))
    if len(packages) != 1:
        raise ValueError("Expected exactly one Windows installer")
    return packages[0]


def main():
    if os.name != "nt":
        raise SystemExit("The Windows installer must be built and tested on Windows")
    # JDK 21 uses WiX 3, preinstalled on windows-2022 hosted runners.
    if not shutil.which("candle.exe"):
        wix = os.environ.get("WIX")
        if wix:
            os.environ["PATH"] = str(Path(wix) / "bin") + os.pathsep + os.environ["PATH"]
    if not shutil.which("candle.exe") or not shutil.which("light.exe"):
        raise SystemExit("WiX 3 candle.exe/light.exe are required by jpackage")
    commit = os.environ.get("SOURCE_SHA", os.environ["GITHUB_SHA"])
    number = int(os.environ["GITHUB_RUN_NUMBER"])
    if number < 1:
        raise ValueError("Expected a positive workflow run number")
    work = Path("build/installer-work")
    manifest = prepare_image(Path("build/kit"), work / "TunuinoConsole", commit)
    previous = build_msi(work / "TunuinoConsole", work / "previous", package_version(number - 1))
    output = Path("build/installer")
    package = build_msi(work / "TunuinoConsole", output, package_version(number))
    setup = output / SETUP_NAME
    package.rename(setup)
    subprocess.run(["pwsh", "-NoProfile", "-File", "misc/install/test_windows_installer.ps1",
                    "-Previous", str(previous.resolve()), "-Installer", str(setup.resolve()),
                    "-Version", package_version(number)], check=True, timeout=480)
    metadata = {"source_commit": commit, "run_id": manifest["run_id"],
                "version": package_version(number), "upgrade_uuid": UPGRADE_UUID,
                "file": SETUP_NAME, "sha256": hashlib.sha256(setup.read_bytes()).hexdigest(),
                "install_upgrade_uninstall_verified": True,
                "user_data_preserved": True, "installed_runtime_verified": True,
                "physical_ecu_validation": False}
    (output / "installer-manifest.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    print(f"Verified installer {setup} version {metadata['version']}", flush=True)


if __name__ == "__main__":
    main()
