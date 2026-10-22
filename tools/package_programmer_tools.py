#!/usr/bin/env python3
"""Create an offline tool layout; optionally copy complete, operator-provided tool distributions."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--destination', type=Path, required=True)
    args = parser.parse_args()
    root = args.destination / 'tools'
    root.mkdir(parents=True, exist_ok=True)
    # Copy the entire distribution, including vendor notices, DLLs and databases.
    # Do not fetch or run unsigned third-party installers during packaging.
    sources = [('TUNUINO_ST_TOOLS', 'STM32_Programmer_CLI', 'bin/STM32_Programmer_CLI.exe'),
               ('TUNUINO_OPENBLT_TOOLS', 'openblt', 'BootCommander.exe')]
    bundled = {}
    for variable, folder, executable in sources:
        source = os.environ.get(variable)
        if source:
            source = Path(source)
            if not (source / executable).is_file():
                raise SystemExit(f'{variable} does not contain {executable}')
            target = root / folder
            shutil.copytree(source, target, dirs_exist_ok=True)
            bundled[folder] = {str(p.relative_to(target)): hashlib.sha256(p.read_bytes()).hexdigest()
                               for p in sorted(target.rglob('*')) if p.is_file()}
    drivers = os.environ.get('TUNUINO_ST_DRIVERS')
    if drivers:
        source = Path(drivers)
        if source.name.lower() != 'silent_st_drivers2.exe' or not source.is_file():
            raise SystemExit('TUNUINO_ST_DRIVERS must name silent_st_drivers2.exe')
        target = args.destination / 'drivers' / source.name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        bundled['drivers'] = {source.name: hashlib.sha256(source.read_bytes()).hexdigest()}
    (root / 'tools-manifest.json').write_text(json.dumps({'bundled': bundled}, indent=2) + '\n')
    (root / 'PROGRAMMERS.txt').write_text(
        'Tunuino offline firmware programming for Windows\n\n'
        'DFU: Install STM32CubeProgrammer and its ST DFU driver, or select the complete\n'
        'STM32_Programmer_CLI/bin/STM32_Programmer_CLI.exe from your existing rusEFI bundle.\n'
        'Keep all DLLs and Data_Base folders with the ST tool. The Console remembers the path.\n'
        'Official installer: https://www.st.com/en/development-tools/stm32cubeprog.html\n\n'
        'Drivers: Install Drivers in the Console accepts drivers/silent_st_drivers2.exe\n'
        'from an existing rusEFI package. Windows may request administrator permission.\n\n'
        'OpenBLT: select the official BootCommander.exe with its libopenblt.dll and other\n'
        'dependencies. Only validated .srec + .srec.properties images are accepted.\n'
        'The current USB bench firmware is DFU-only and replaces the flash bootloader.\n'
        'OpenBLT mode requires the ECU already in its bootloader; auto reboot is not implemented.\n'
        'Official tools: https://www.feaser.com/openblt/doku.php?id=manual:bootcommander\n\n'
        'Optional portable layout: tools/STM32_Programmer_CLI/bin/STM32_Programmer_CLI.exe\n'
        'and tools/openblt/BootCommander.exe, beside the TunuinoConsole.exe application.\n'
        'Third-party tools are included only when their full distributions are supplied at build time.\n',
        encoding='utf-8')
    print('Programmer tool layout prepared; included distributions:', ', '.join(bundled) or 'none (use local installed tools)')


if __name__ == '__main__':
    main()
