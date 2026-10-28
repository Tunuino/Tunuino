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
    assert section.index('nPages = 4') < section.index('blockingFactor = 100')
    assert '[UiDialogs]' in text and '[UserDefined]' not in text
    ini=configparser.ConfigParser(interpolation=None,strict=False);ini.read_string('[General]\n'+text)
    assert ini['TunerStudio'].getint('iniVersion')==11
    assert '128, [0:1]' in ini['Constants']['engineProfileFormat']
    assert '156, [0:1]' in ini['Constants']['engineOperation']
    import re
    feature_fields=json.loads((ROOT/'firmware/tunerstudio/feature_fields.json').read_text())
    java=(ROOT/'java_console/src/main/java/io/tunuino/calibration/FeatureConfiguration.java').read_text()
    declarations={m[0]:tuple(map(int,m[1:])) for m in re.findall(r'(\w+)\("[^"\n]+","[^"\n]+",(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+)\)',java)}
    assert set(declarations)=={f['name'] for f in feature_fields}
    occupied=set()
    for f in feature_fields:
        assert declarations[f['name']]==tuple(f[k] for k in ('offset','decimals','min','max','reference'))
        width=1 if f['offset']<76 else 2
        for offset in range(f['offset'],f['offset']+width):
            assert 72<=offset<240 and offset not in occupied
            occupied.add(offset)
        assert f'feature{f["name"]}' in ini['Constants']
        if '(ms)' in f['label']:assert '\"ms\"' in ini['Constants'][f'feature{f["name"]}']
    assert ini['OutputChannels']['ochGetCommand']=='"O%2o%2c"'
    assert ini['OutputChannels']['engineState']=='scalar, U08, 31, "", 1, 0'
    for name in ('engineMode','engineAutonomous','engineFault'):
        assert 'engineState' in ini['OutputChannels'][name]
    assert 'engineProfileFormat' in ini['Constants'] and 'enginePolicy' in ini['Constants']
    assert ini['Constants'].getint('nPages')==4 and ini['Constants']['pageSize']=='256, 576, 576, 256'
    assert ini['Constants'].getint('blockingFactor')>=ini['OutputChannels'].getint('ochBlockSize')
    assert ini['Constants']['endianness']=='big' and ini['OutputChannels'].getint('ochBlockSize')==100
    for field,prefix in [('pageReadCommand','r'),('pageChunkWrite','w'),('burnCommand','b'),('crc32CheckCommand','k')]:
        assert ini['Constants'][field].count('"'+prefix)==4
    with tempfile.TemporaryDirectory(prefix='tunuino-io-') as directory:
        work=Path(directory);exe=work/('io.exe' if os.name=='nt' else 'io')
        sources=sorted((ROOT/'firmware/src').glob('*.c'))
        subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-pedantic','-I'+str(ROOT/'firmware/include'),
            *map(str,sources),str(ROOT/'unit_tests/test_device_io.c'),'-lm','-o',str(exe)],check=True,timeout=30)
        lines=subprocess.check_output([str(exe)],text=True,timeout=20).splitlines();assert lines[-1]=='OK'
        vectors={name:bytes.fromhex(value) for name,value in (line.split() for line in lines[:-1])}
        cam=decode(vectors['cam_phase_valid'])
        assert len(cam)==31 and cam[:3]==bytes((1,7,0))
        assert int.from_bytes(cam[3:7],'big')==42125
        assert int.from_bytes(cam[7:15],'big')==4294968000
        assert int.from_bytes(cam[15:23],'big')==4294969000
        assert cam[27:]==bytes((0,60,2,0))
        lost=decode(vectors['cam_phase_lost'])
        assert lost[:3]==bytes((1,0,0)) and not any(lost[3:23])
        assert decode(vectors['signature'])==json.loads(ini['TunerStudio']['signature']).encode()
        assert decode(vectors['signature_q'])==decode(vectors['signature'])
        assert decode(vectors['version'])==b'Tunuino 0.2.0 Proteus v0.7 engine configuration\0'
        assert decode(vectors['ts_protocol'])==b'001'
        assert decode(vectors['ts_test'])==b'\xff'
        assert decode(vectors['ts_seconds'])==bytes(2)
        assert decode(vectors['ts_can_id'])==bytes(1)
        assert decode(vectors['ts_capabilities'])==bytes((2,0,100,0,64))
        runtime=decode(vectors['runtime_configured']);assert len(runtime)==100 and runtime[:7]==b'TUNUINO'
        default_runtime=decode(vectors['runtime_default'])
        assert decode(vectors['runtime_slice_default'])==default_runtime[38:50]
        assert decode(vectors['runtime_last_byte'])==default_runtime[99:100]
        assert decode(vectors['runtime_slice_configured'])==runtime[92:100]
        idle=decode(vectors['engine_preview_disabled'])
        assert len(idle)==19 and idle[0]==1 and idle[1]==0 and idle[2]&0x07==0x07
        valid=decode(vectors['engine_preview_valid'])
        assert len(valid)==19 and valid[0:3]==bytes((1,1,0))
        assert int.from_bytes(valid[3:5],'big')==10000
        assert int.from_bytes(valid[5:7],'big',signed=True)==-500
        assert int.from_bytes(valid[7:11],'big')==1234500
        assert int.from_bytes(valid[11:15],'big')==100000
        dryrun=decode(vectors['engine_timing_valid'])
        assert len(dryrun)==31 and dryrun[:3]==bytes((1,1,0))
        assert int.from_bytes(dryrun[3:7],'big')==1234500
        assert int.from_bytes(dryrun[7:11],'big')>0
        assert int.from_bytes(dryrun[11:15],'big')==2*int.from_bytes(dryrun[7:11],'big')
        assert int.from_bytes(dryrun[19:23],'big')==10000
        assert int.from_bytes(dryrun[23:27],'big',signed=True)>0
        assert decode(vectors['engine_timing_disabled'])[1]==0
        assert decode(vectors['engine_timing_no_sync'])[2]&0x08
        nosync=decode(vectors['engine_preview_no_sync'])
        assert nosync[1]==0 and nosync[2]&0x09==0x09
        noadc=decode(vectors['engine_preview_adc_invalid'])
        assert noadc[1]==0 and noadc[2]&0x12==0x12
        expected={'hardwareRevision':7,'firmwareMajor':0,'firmwareMinor':2,'firmwarePatch':0,'capabilities':15,
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
