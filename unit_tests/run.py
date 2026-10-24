#!/usr/bin/env python3
"""Run the portable unit/integration suite from any working directory."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', help='C compiler executable (default: CC or cc)')
args = parser.parse_args()
env = os.environ.copy()
if args.compiler:
    env['CC'] = args.compiler
raise SystemExit(subprocess.call([sys.executable, str(ROOT / 'tools/check_core.py')], cwd=ROOT, env=env))
