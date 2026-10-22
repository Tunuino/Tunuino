#!/usr/bin/env python3
"""Exercise firmware validation and process/tool failure paths without USB hardware."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    with tempfile.TemporaryDirectory(prefix='tunuino-maintenance-') as work:
        javac = ['javac'] if shutil.which('javac') else ['java', '--module', 'jdk.compiler/com.sun.tools.javac.Main']
        sources = sorted((ROOT / 'console/src/main/java/io/tunuino/maintenance').glob('*.java'))
        sources.append(ROOT / 'console/src/test/java/io/tunuino/FirmwareProgrammingChecks.java')
        subprocess.run(javac + ['-encoding', 'UTF-8', '--release', '17', '-d', work] + list(map(str, sources)), check=True)
        subprocess.run(['java', '-cp', work, 'io.tunuino.FirmwareProgrammingChecks'], check=True, timeout=40)


if __name__ == '__main__':
    main()
