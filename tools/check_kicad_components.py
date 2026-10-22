#!/usr/bin/env python3
# Copyright (c) 2026 Tunuino.
from copy import deepcopy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from compare_kicad_components import compare

ROOT=b'''(kicad_sch (version 20230121) (uuid ROOT) (title_block (rev X1))
(sheet (uuid LEAF) (property "Sheetfile" "mcu.kicad_sch")))'''
LEAF=b'''(kicad_sch (version 20230121) (title_block (rev X1))
(lib_symbols (symbol ignored (property "Reference" "U1") (property "Value" "library template")))
(symbol (property "Reference" "U1") (property "Value" "STM32F427ZGTx")
(property "PN" "STM32F767ZIT6") (property "Footprint" "Package:QFP")
(instances (project board (path "/ROOT/LEAF" (reference U1) (unit 1))))))'''
PCB={'format':'tunuino-pcb-membership-v1','family':'proteus','revision':None,
    'source':{'repository':'Example/board','commit':'a'*40,'path':'board.kicad_pcb'},
    'selected_components':[{'ref':'U1','value':'STM32F427ZGTx','footprint':'Package:QFP'}]}


class Checks(unittest.TestCase):
    def report(self,pcb=None,root=ROOT,leaf=LEAF,**paths):
        return compare(pcb or PCB,root,leaf,root_path=paths.get('root_path','board.kicad_sch'),sheet_path=paths.get('sheet_path','mcu.kicad_sch'))

    def test_mcu_conflict_survives_matching_symbol_fields(self):
        r=self.report()
        self.assertEqual(r['result'],'requires_review')
        self.assertTrue(r['comparisons'][0]['fields_match'])
        self.assertEqual(r['comparisons'][0]['schematic']['part_number'],'STM32F767ZIT6')
        self.assertEqual(r['findings'][0]['code'],'MCU_SYMBOL_PART_REQUIRES_REVIEW')
        self.assertFalse(r['mcu_compatibility_validated'])
        self.assertFalse(r['firmware_pin_definitions_generated'])
        self.assertIsNone(r['pcb_revision'])
        self.assertEqual(r['root_schematic_revision'],'X1')
        self.assertEqual(len(r['source']['leaf_schematic']['git_blob_sha1']),40)

    def test_matching_fields_never_approve_hardware(self):
        r=self.report(leaf=LEAF.replace(b'STM32F767ZIT6',b'STM32F427ZGT6'))
        self.assertEqual(r['result'],'selected_fields_match')
        self.assertEqual(r['validation'],'unreviewed')
        self.assertFalse(r['mcu_compatibility_validated'])
        self.assertFalse(r['firmware_pin_definitions_generated'])

    def test_missing_ambiguous_and_mismatching_symbols(self):
        cases=[LEAF.replace(b'(property "PN" "STM32F767ZIT6")',b''),
            LEAF.replace(b'(property "Reference" "U1")\n',b'(property "Reference" "U2")\n'),
            LEAF.replace(b'(property "Footprint" "Package:QFP")',b'(property "Footprint" "Package:Other")'),
            LEAF.replace(b'(rev X1)',b'(rev X2)'),
            LEAF.replace(b'(path "/ROOT/LEAF"',b'(path "/ROOT" (reference U1) (unit 1)) (path "/ROOT/LEAF"')]
        for leaf in cases:
            with self.subTest(leaf=leaf[-100:]):
                r=self.report(leaf=leaf)
                self.assertEqual(r['result'],'requires_review')
                self.assertFalse(r['mcu_compatibility_validated'])

    def test_reject_unproven_hierarchy_or_instances(self):
        for root,leaf in ((ROOT.replace(b'mcu.kicad_sch',b'other.kicad_sch'),LEAF),
                (ROOT,LEAF.replace(b'/ROOT/LEAF',b'/ROOT/OTHER')),
                (ROOT,LEAF.replace(b'project board',b'project other')),
                (ROOT,LEAF.replace(b'version 20230121',b'version 20250101')),
                (ROOT,LEAF.replace(b'(property "PN"',b'(property "Reference" "U1") (property "PN"'))):
            with self.subTest(root=root,leaf=leaf[-60:]),self.assertRaises(ValueError):self.report(root=root,leaf=leaf)
        pcb=deepcopy(PCB);pcb['selected_components']*=2
        with self.assertRaises(ValueError):self.report(pcb=pcb)
        for path in ('../mcu.kicad_sch','/mcu.kicad_sch'):
            with self.assertRaises(ValueError):self.report(sheet_path=path)

    def test_cli_does_not_overwrite_or_write_invalid_input(self):
        with tempfile.TemporaryDirectory(prefix='tunuino-components-') as directory:
            base=Path(directory);pcb=base/'pcb.json';root=base/'root';leaf=base/'leaf';output=base/'comparison.json'
            pcb.write_text(json.dumps(PCB));root.write_bytes(ROOT);leaf.write_bytes(LEAF)
            command=[sys.executable,str(Path(__file__).with_name('compare_kicad_components.py')),
                '--pcb-report',str(pcb),'--root-schematic',str(root),'--root-path','board.kicad_sch',
                '--sheet-schematic',str(leaf),'--sheet-path','mcu.kicad_sch','--output',str(output)]
            subprocess.run(command,check=True,capture_output=True,timeout=5)
            original=output.read_bytes()
            self.assertNotEqual(subprocess.run(command,capture_output=True,timeout=5).returncode,0)
            self.assertEqual(output.read_bytes(),original)
            output.unlink();leaf.write_bytes(b'(bad')
            self.assertNotEqual(subprocess.run(command,capture_output=True,timeout=5).returncode,0)
            self.assertFalse(output.exists())


if __name__=='__main__':
    unittest.main()
