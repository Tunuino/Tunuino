#!/usr/bin/env python3
"""Report developer prerequisites without installing software or changing a machine."""
import shutil
import subprocess
import sys

print('Python:', sys.version.split()[0])
missing = []
for name, command in [('C compiler', ['cc', '--version']), ('Java', ['java', '-version'])]:
    if shutil.which(command[0]):
        result = subprocess.run(command, text=True, capture_output=True, timeout=10)
        print(name + ':', (result.stdout + result.stderr).splitlines()[0])
        if result.returncode: missing.append(name)
    else:
        print(name + ': missing'); missing.append(name)
print('Optional ARM compiler:', shutil.which('arm-none-eabi-gcc') or 'not installed')
print('Optional STM32 programmer:', shutil.which('STM32_Programmer_CLI') or 'not on PATH')
raise SystemExit(bool(missing))
