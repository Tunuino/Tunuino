#!/usr/bin/env python3
"""Independent CRC/layout and recovery checks against the actual simulator executable."""
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import zlib
from build_simulator import build


def frame(body): return len(body).to_bytes(2,'big') + body + zlib.crc32(body).to_bytes(4,'big')


def decode(data):
    result = []
    while data:
        count = int.from_bytes(data[:2],'big')
        assert 1 <= count <= 4096 and len(data) >= count+6
        body = data[2:count+2]
        assert zlib.crc32(body) == int.from_bytes(data[count+2:count+6],'big')
        result.append(body); data = data[count+6:]
    return result


def main():
    with tempfile.TemporaryDirectory(prefix='tunuino-simulator-') as directory:
        exe = build(Path(directory))
        for scenario in ('normal','stall','sensor-loss','wrap'):
            result = subprocess.run([str(exe),'--stdio','--scenario',scenario],input=frame(b'TUNUINO?')+frame(b'TUNUINO:L')*24,
                capture_output=True,check=True,timeout=15)
            assert b'SIMULATOR' in result.stderr
            responses = decode(result.stdout)
            assert len(responses)==25 and responses[0][20:]==b'SIMULATOR001'
            samples = responses[1:]
            ready = [b for b in samples if len(b)==28]
            assert len(ready)>10 and all(b[0:4]==bytes((0,1,15,0)) for b in ready)
            times = [int.from_bytes(b[4:12],'big') for b in ready]
            assert all(a<b for a,b in zip(times,times[1:]))
            assert all(abs(int.from_bytes(b[12:16],'big')/1000-1000)<1 for b in ready)
            assert len(samples[-1])==28
            assert any(b==b'\x01' for b in samples) == (scenario in ('stall','sensor-loss'))
            if scenario=='wrap': assert times[0]>4_000_000
        result = subprocess.run([str(exe),'--stdio','--bench'],input=frame(b'S')+frame(b'A')+frame(b'W'),capture_output=True,check=True,timeout=10)
        bodies = decode(result.stdout)
        assert bodies[0][1:]==b'Tunuino Proteus v0.7 F427 USB bench v1\0' and len(bodies[1])==32 and bodies[2]==b'\x83'
        corrupt = bytearray(frame(b'TUNUINO?'));corrupt[-1]^=1
        bad = subprocess.run([str(exe),'--stdio'],input=corrupt,capture_output=True,timeout=10)
        assert bad.returncode != 0 and not bad.stdout
        for args in (['--rpm','NaN'], ['--family','other'], ['--scenario','unknown'], ['--ve','missing.tnmap']):
            result = subprocess.run([str(exe),'--stdio',*args],input=b'',capture_output=True,timeout=10)
            assert result.returncode != 0 and not result.stdout
    print('Simulator independent checks: real decoder RPM, stall/sensor recovery, counter wrap, bench INI, CRC rejection and invalid configuration passed.')


if __name__ == '__main__': main()
