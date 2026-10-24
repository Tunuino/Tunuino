#!/usr/bin/env python3
"""Build the native, synthetic desktop simulator with the actual portable firmware core."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def build(output, debug=False):
    output = Path(output).resolve(); output.mkdir(parents=True, exist_ok=True)
    compiler = os.environ.get('CC', 'gcc' if os.name == 'nt' else 'cc')
    if not shutil.which(compiler): raise SystemExit('C compiler missing: ' + compiler)
    exe = output / ('tunuino-simulator.exe' if os.name == 'nt' else 'tunuino-simulator')
    sources = sorted((ROOT / 'firmware/src').glob('*.c')) + [ROOT/'simulator/model.c', ROOT/'simulator/main.c']
    flags = ['-O0', '-g'] if debug else ['-O2']
    subprocess.run([compiler,'-std=c11','-Wall','-Wextra','-Werror','-pedantic',*flags,
        '-I'+str(ROOT/'firmware/include'),*map(str,sources),'-lm','-o',str(exe)], check=True, timeout=90)
    commit = os.environ.get('GITHUB_SHA')
    if not commit:
        result = subprocess.run(['git','rev-parse','HEAD'],cwd=ROOT,text=True,capture_output=True)
        commit = result.stdout.strip() if result.returncode == 0 else 'development'
    (output/'manifest.json').write_text(json.dumps({'source_commit':commit,'synthetic':True,
        'engine_outputs':False,'protocol':'Tunuino native; optional TunerStudio bench profile',
        'binary':exe.name,'sha256':hashlib.sha256(exe.read_bytes()).hexdigest()},indent=2)+'\n')
    shutil.copy2(ROOT/'LICENSE',output/'LICENSE.txt')
    return exe


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'build/simulator')
    parser.add_argument('--debug',action='store_true')
    args = parser.parse_args()
    print(build(args.output,args.debug))
