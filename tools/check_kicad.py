#!/usr/bin/env python3
# Copyright (c) 2026 Tunuino.
"""Verify connector membership extraction without hardware assumptions."""
from copy import deepcopy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET
import import_kicad_netlist as importer

XML = b'''<export version="E"><design><sheet number="1"><title_block><rev>X1</rev></title_block></sheet></design>
<components><comp ref="J1"><value>Connector</value></comp><comp ref="R1"><value>100</value></comp>
<comp ref="U1"><value>Test module</value></comp></components><nets>
<net code="1" name="external"><node ref="J1" pin="1"/><node ref="R1" pin="1"/></net>
<net code="2" name="internal"><node ref="R1" pin="2"/><node ref="U1" pin="8"/></net>
<net code="3" name="direct"><node ref="J1" pin="2"/><node ref="U1" pin="9" pinfunction="label_PA11"/></net>
</nets></export>'''
META = dict(family="uaefi", revision="X1", repository="Example/board", commit="a" * 40,
    source_path="board.net", connectors=["J1"], targets=["U1"])


class Checks(unittest.TestCase):
    def report(self, data=XML, **changes):
        options = dict(META); options.update(changes)
        return importer.import_netlist(data, **options)

    def test_exact_membership_and_provenance(self):
        r = self.report()
        self.assertEqual(r["validation"], "unreviewed")
        self.assertFalse(r["firmware_pin_definitions_generated"])
        self.assertEqual(r["source_counts"], dict(components=3, nets=3, nodes=6))
        self.assertEqual(len(r["source"]["sha256"]), 64)
        self.assertEqual(len(r["source"]["git_blob_sha1"]), 40)
        nets = {n["name"]:n for n in r["nets"]}
        self.assertEqual(nets["external"]["selected_target_nodes"], [])  # Never jump through R1.
        self.assertEqual(nets["direct"]["selected_target_nodes"], [{"ref":"U1", "pin":"9", "pinfunction":"label_PA11"}])
        self.assertEqual(self.report(XML.replace(b'version="E"',b'version="D"'))["pins"],r["pins"])
        self.assertEqual(self.report(XML, targets=[])["nets"][0]["selected_target_nodes"],[])
        root = ET.fromstring(XML); root.find('nets').append(deepcopy(root.find('nets/net')))
        with self.assertRaises(ValueError): self.report(ET.tostring(root))

    def test_invalid_metadata(self):
        for changes in (dict(family="other"),dict(revision="different"),dict(repository="../board"),
                dict(commit="main"),dict(source_path="../board.net"),dict(source_path="/board.net"),
                dict(source_path="C:\\board.net"),dict(connectors=[]),dict(connectors=["J1","J1"]),
                dict(targets=["U1","U1"]),dict(targets=["J1"]),dict(connectors=["J99"])):
            with self.subTest(changes=changes), self.assertRaises(ValueError): self.report(**changes)

    def test_corruption_and_ambiguity(self):
        for data in (b'',b'<broken',b'<!DOCTYPE export><export/>',b'<!ENTITY x "v">',XML.decode().encode('utf-16'),
                XML.replace(b'version="E"',b'version="F"'),XML.replace(b'ref="R1" pin="1"',b'ref="missing" pin="1"'),
                XML.replace(b'ref="R1" pin="2"',b'ref="R1" pin="1"'),
                XML.replace(b'code="2"',b'code="01"'),XML.replace(b'name="internal"',b'name="external"'),
                XML.replace(b'pin="9"',b'pin=""'),XML.replace(b'<nets>',b'<nets/><nets>'),
                XML.replace(b'<comp ref="R1">',b'<comp ref="J1">')):
            with self.subTest(data=data[:50]), self.assertRaises(ValueError): self.report(data)
        with patch.object(importer,'MAX_BYTES',10), self.assertRaises(ValueError): self.report()

    def test_order_determinism(self):
        original = self.report()
        root = ET.fromstring(XML)
        for tag in ('components','nets'):
            parent = root.find(tag); parent[:] = reversed(list(parent))
        changed = self.report(ET.tostring(root))
        self.assertEqual(original['pins'],changed['pins'])
        self.assertEqual(original['nets'],changed['nets'])
        self.assertEqual(original['selected_components'],changed['selected_components'])

    def test_cli_preserves_existing_and_rejects_before_create(self):
        with tempfile.TemporaryDirectory(prefix='tunuino-kicad-') as directory:
            source = Path(directory)/'input.net'; output = Path(directory)/'report.json'
            source.write_bytes(XML)
            command = [sys.executable,str(Path(importer.__file__).resolve()),str(source),
                '--family','uaefi','--revision','X1','--repository','Example/board','--commit','a'*40,
                '--source-path','board.net','--connector','J1','--target','U1','--output',str(output)]
            subprocess.run(command,check=True,capture_output=True,timeout=5)
            saved = output.read_bytes()
            self.assertEqual(json.loads(saved)['validation'],'unreviewed')
            self.assertNotEqual(subprocess.run(command,capture_output=True,timeout=5).returncode,0)
            self.assertEqual(output.read_bytes(),saved)
            output.unlink(); source.write_bytes(b'<invalid')
            self.assertNotEqual(subprocess.run(command,capture_output=True,timeout=5).returncode,0)
            self.assertFalse(output.exists())


if __name__ == '__main__':
    unittest.main()
