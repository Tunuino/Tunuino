#!/usr/bin/env python3
"""Common developer entry point; never flashes hardware or installs drivers."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('command', choices=['check', 'simulator', 'generate', 'environment', 'tools', 'console'])
args, rest = parser.parse_known_args()
scripts = {'check':'unit_tests/run.py', 'simulator':'tools/build_simulator.py',
           'generate':'firmware/tunerstudio/generate.py', 'environment':'misc/install/check_environment.py'}
if args.command in scripts:
    command = [sys.executable, str(ROOT / scripts[args.command]), *rest]
elif args.command == 'console':
    if rest: parser.error('console does not accept extra arguments')
    command = [str(ROOT / ('gradlew.bat' if os.name == 'nt' else 'gradlew')), ':java_console:run']
else:
    libraries = ROOT / 'java_tools/build/install/java_tools/lib'
    if not libraries.is_dir():
        parser.error('Build tools first: ./gradlew :java_tools:installDist')
    command = ['java', '-cp', str(libraries / '*'), 'io.tunuino.tools.Tools', *rest]
raise SystemExit(subprocess.call(command, cwd=ROOT))
