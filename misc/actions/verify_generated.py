#!/usr/bin/env python3
"""CI entry point for deterministic generated definitions."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
raise SystemExit(subprocess.call([sys.executable, str(ROOT / 'firmware/tunerstudio/generate.py'), '--check'], cwd=ROOT))
