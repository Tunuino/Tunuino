#!/usr/bin/env python3
"""Exercise firmware validation and process/tool failure paths without USB hardware."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--jar', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='tunuino-maintenance-') as work:
        javac = ['javac'] if shutil.which('javac') else ['java', '--module', 'jdk.compiler/com.sun.tools.javac.Main']
        sources = [] if args.jar else sorted((ROOT / 'java_console/src/main/java/io/tunuino/maintenance').glob('*.java'))
        sources.append(ROOT / 'java_console/src/test/java/io/tunuino/FirmwareProgrammingChecks.java')
        jar = args.jar.resolve() if args.jar else None
        classpath = str(jar) + os.pathsep + work if jar else work
        extra = ['-cp', str(jar)] if jar else []
        subprocess.run(javac + ['-encoding', 'UTF-8', '--release', '17', '-d', work] + extra + list(map(str, sources)), check=True)
        subprocess.run(['java', '-cp', classpath, 'io.tunuino.FirmwareProgrammingChecks']
                       + ([os.environ['GITHUB_SHA']] if jar else []), check=True, timeout=40)


if __name__ == '__main__':
    main()
