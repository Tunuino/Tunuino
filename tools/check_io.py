#!/usr/bin/env python3
"""Cross-check the IO INI with actual C responses and journal interruption tests."""
import configparser
import csv
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import zlib
from ci_package_kit import validate_tunerstudio

ROOT=Path(__file__).resolve().parents[1]
NAME='tunuino_proteus_v07_f427_io.ini'


def decode(packet):
    size=int.from_bytes(packet[:2],'big');body=packet[2:-4]
    assert size==len(body) and len(packet)==size+6 and body[0]==0
    assert zlib.crc32(body)==int.from_bytes(packet[-4:],'big')
    return body[1:]


def main():
    content=(ROOT/'firmware/tunerstudio/generated'/NAME).read_bytes()
    text=content.decode('ascii')
    section=text.split('\n[Constants]\n',1)[1].split('\n[',1)[0]
    assert 'baudRate =' not in section
    assert section.index('nPages = 3') < section.index('blockingFactor = 64')
    assert '[UiDialogs]' in text and '[UserDefined]' not in text
    ini=configparser.ConfigParser(interpolation=None,strict=False);ini.read_string('[General]\n'+text)
    assert ini['TunerStudio'].getint('iniVersion')==2
    assert ini['Constants'].getint('nPages')==3 and ini['Constants']['pageSize']=='256, 160, 160'
    assert ini['Constants']['endianness']=='big' and ini['OutputChannels'].getint('ochBlockSize')==100
    for field,prefix in [('pageReadCommand','r'),('pageChunkWrite','w'),('burnCommand','b'),('crc32CheckCommand','k')]:
        assert ini['Constants'][field].count('"'+prefix)==3
    with tempfile.TemporaryDirectory(prefix='tunuino-io-') as directory:
        work=Path(directory);exe=work/('io.exe' if os.name=='nt' else 'io')
        sources=sorted((ROOT/'firmware/src').glob('*.c'))
        subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-pedantic','-I'+str(ROOT/'firmware/include'),
            *map(str,sources),str(ROOT/'unit_tests/test_device_io.c'),'-lm','-o',str(exe)],check=True,timeout=30)
        lines=subprocess.check_output([str(exe)],text=True,timeout=20).splitlines();assert lines[-1]=='OK'
        vectors={name:bytes.fromhex(value) for name,value in (line.split() for line in lines[:-1])}
        assert decode(vectors['signature'])==json.loads(ini['TunerStudio']['signature']).encode()+b'\0'
        runtime=decode(vectors['runtime_configured']);assert len(runtime)==100 and runtime[:7]==b'TUNUINO'
        expected={'hardwareRevision':7,'firmwareMajor':0,'firmwareMinor':1,'firmwarePatch':0,'capabilities':15,
            'uptimeSeconds':1234,'uptimeMillis':567,'validMask':15,'ioStatus':13,'rpmValue':1234.5,
            'mapValue':100.0,'lambdaValue':1.0,'coolantValue':50.0,'adcReferenceMv':3300,'triggerTooth':12,'triggerErrors':3}
        for i in range(16):expected['raw'+str(i+1)]=2000
        for name,value in expected.items():
            fields=next(csv.reader([ini['OutputChannels'][name]],skipinitialspace=True));kind=fields[1];offset=int(fields[2])
            width={'U08':1,'U16':2,'U32':4,'S32':4}[kind]
            actual=int.from_bytes(runtime[offset:offset+width],'big',signed=kind.startswith('S'))*float(fields[4])+float(fields[5])
            assert abs(actual-value)<1e-8,(name,actual,value)
        assert decode(vectors['partial_no_rpm'])[:3]==bytes((2,14,0))
        fixture=work/'firmware';fixture.mkdir();path=fixture/NAME;path.write_bytes(content)
        metadata={'artifacts_sha256':{NAME:hashlib.sha256(content).hexdigest()},'tunerstudio':{
            'ini_file':NAME,'signature':json.loads(ini['TunerStudio']['signature']),'output_bytes':100,'read_only':False}}
        assert validate_tunerstudio(work,metadata)==metadata['tunerstudio']
        for key,value in [('output_bytes',31),('read_only',True),('signature','old firmware')]:
            previous=metadata['tunerstudio'][key];metadata['tunerstudio'][key]=value
            try:validate_tunerstudio(work,metadata)
            except ValueError:pass
            else:raise AssertionError('Mixed IO definition accepted')
            metadata['tunerstudio'][key]=previous
    print('F427 IO INI: C-derived signature/layout/scales, writable pages, partial data and publication gates passed. Host models; TunerStudio/device execution pending.')


if __name__=='__main__':main()
