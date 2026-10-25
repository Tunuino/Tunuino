#!/usr/bin/env python3
"""Validate native configuration diagnostics with independent frame/CRC decoding."""
import os
from pathlib import Path
import subprocess
import tempfile
import zlib

ROOT=Path(__file__).resolve().parents[1]


def main():
    with tempfile.TemporaryDirectory(prefix='tunuino-config-') as work:
        exe=Path(work)/('check.exe' if os.name=='nt' else 'check')
        subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-pedantic',
            '-I'+str(ROOT/'firmware/include'),*map(str,sorted((ROOT/'firmware/src').glob('*.c'))),
            str(ROOT/'unit_tests/test_configuration.c'),'-lm','-o',str(exe)],check=True)
        lines=subprocess.check_output([str(exe)],text=True,timeout=15).splitlines();assert lines[-1]=='OK'
        frame=bytes.fromhex(lines[0].split()[1]);body=frame[2:-4]
        assert len(frame)==22 and int.from_bytes(frame[:2],'big')==16
        assert zlib.crc32(body)==int.from_bytes(frame[-4:],'big')
        assert body[:4]==bytes([0,1,1,0]) and int.from_bytes(body[4:8],'big')==255 and int.from_bytes(body[8:12],'big')==1
    print('Configuration diagnostics: all eight validation groups, combined failures, independent CRC/layout and no flash/state changes passed.')


if __name__=='__main__':main()
