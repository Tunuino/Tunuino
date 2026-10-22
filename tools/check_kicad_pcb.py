#!/usr/bin/env python3
# Copyright (c) 2026 Tunuino.
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import kicad_sexpr as sexpr
from import_kicad_pcb import import_pcb

PCB = b'''(kicad_pcb (version 20171130) (net 0 "") (net 1 "USB") (net 2 "GND")
(module Conn (fp_text reference J1) (fp_text value Connector)
(pad 1 thru_hole circle (net 1 USB)) (pad 2 thru_hole circle (net 2 GND))
(pad 2 thru_hole circle (net 2 GND)) (pad 3 thru_hole circle))
(module Chip (fp_text reference U1) (fp_text value MCU)
(pad 9 smd rect (net 1 USB)))
(module "" (fp_text reference "") (fp_text value "") (pad 1 smd rect (net 2 GND)))
(module "" (fp_text reference "") (fp_text value "") (pad 1 smd rect (net 2 GND))))'''
META = dict(family='proteus',repository='Example/board',commit='a'*40,source_path='board.kicad_pcb',connectors=['J1'],targets=['U1'])


class Checks(unittest.TestCase):
    def report(self,data=PCB,**changes):
        options=dict(META);options.update(changes)
        return import_pcb(data,**options)

    def test_exact_membership_and_anonymous_count(self):
        r=self.report(); nets={n['name']:n for n in r['nets']}
        self.assertEqual(r['validation'],'unreviewed')
        self.assertFalse(r['firmware_pin_definitions_generated'])
        self.assertIsNone(r['revision'])
        self.assertEqual(r['revision_status'],'not_declared_in_pcb')
        self.assertEqual(nets['USB']['selected_target_nodes'],[dict(ref='U1',pin='9',pad_instances=1)])
        self.assertEqual(nets['GND']['anonymous_endpoint_count'],2)
        self.assertEqual(r['pins'][1]['pad_instances'],2)
        self.assertIsNone(r['pins'][2]['net'])
        self.assertEqual(r['source_counts']['anonymous_electrical_footprints'],2)
        self.assertEqual(self.report(PCB.replace(b'(version 20171130)',b'(version 20171130) (title_block (rev "X1"))'))['revision'],'X1')

    def test_rejections(self):
        cases=[b'',b'(broken',b'(kicad_pcb (version 20240108))',PCB+b' (other)',
            PCB.replace(b'(net 1 USB)',b'(net 1 other)',1),
            PCB.replace(b'(net 1 USB)',b'(net 99 other)',1),
            PCB.replace(b'(pad 2 thru_hole circle (net 2 GND))',b'(pad 2 thru_hole circle (net 1 USB))',1),
            PCB.replace(b'(fp_text reference U1)',b'(fp_text reference J1)'),
            PCB.replace(b'(net 2 "GND")',b'(net 1 "GND")'),
            PCB.replace(b'(net 2 "GND")',b'(net 2 "USB")'),
            PCB.replace(b'(version 20171130)',b'(version 20171130) (title_block (rev))'),
            PCB.replace(b'(net 2 "GND")',b'(net (bad) "GND")')]
        for data in cases:
            with self.subTest(data=data[:60]),self.assertRaises(ValueError): self.report(data)
        for changes in (dict(commit='main'),dict(source_path='../x'),dict(repository='../x'),dict(connectors=['J99']),dict(targets=['J1']),dict(connectors=[]),dict(targets=['U1','U1'])):
            with self.subTest(changes=changes),self.assertRaises(ValueError): self.report(**changes)

    def test_parser_limits_and_string_boundaries(self):
        self.assertEqual(sexpr.parse(b'(x "a (b) \\"quoted\\"" "c\\\\d")'),['x','a (b) "quoted"','c\\d'])
        for bad in (b'()',b')',b'(x "unfinished)',b'x',b'(x) (y)',b'(x\0)',b'\xff'):
            with self.subTest(bad=bad),self.assertRaises(ValueError): sexpr.parse(bad)
        with patch.object(sexpr,'MAX_DEPTH',2),self.assertRaises(ValueError): sexpr.parse(b'(a (b (c)))')
        with patch.object(sexpr,'MAX_NODES',2),self.assertRaises(ValueError): sexpr.parse(b'(a b c)')
        with patch.object(sexpr,'MAX_BYTES',3),self.assertRaises(ValueError): sexpr.parse(b'(abc)')

    def test_cli_preserves_existing(self):
        with tempfile.TemporaryDirectory(prefix='tunuino-pcb-') as directory:
            source=Path(directory)/'board.kicad_pcb';output=Path(directory)/'report.json';source.write_bytes(PCB)
            command=[sys.executable,str(Path(__file__).with_name('import_kicad_pcb.py')),str(source),
                '--family','proteus','--repository','Example/board','--commit','a'*40,'--source-path','board.kicad_pcb',
                '--connector','J1','--target','U1','--output',str(output)]
            subprocess.run(command,check=True,capture_output=True,timeout=5)
            original=output.read_bytes();self.assertEqual(json.loads(original)['validation'],'unreviewed')
            self.assertNotEqual(subprocess.run(command,capture_output=True,timeout=5).returncode,0)
            self.assertEqual(output.read_bytes(),original)
            output.unlink();source.write_bytes(b'(broken')
            self.assertNotEqual(subprocess.run(command,capture_output=True,timeout=5).returncode,0)
            self.assertFalse(output.exists())


if __name__=='__main__':
    unittest.main()
