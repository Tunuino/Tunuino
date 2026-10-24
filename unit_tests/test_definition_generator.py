"""Generator rejects ambiguous layouts and broken UI references before a firmware build."""
import copy
import json
from pathlib import Path
import runpy
import unittest

FOLDER = Path(__file__).resolve().parents[1] / 'firmware/tunerstudio'
render = runpy.run_path(str(FOLDER / 'generate.py'))['render']


class DefinitionChecks(unittest.TestCase):
    def setUp(self):
        self.schema = json.loads((FOLDER / 'schema.json').read_text())
        self.template = (FOLDER / 'tunerstudio.template.ini').read_text()
        self.fragments = {key:(FOLDER / name).read_text() for key,name in {
            'gauges':'gauge_declarations.ini', 'front_page':'main_page_gauges.ini', 'menu':'top_level_menu.ini'}.items()}

    def test_rejects_ambiguous_layout(self):
        for mutation in ('overlap', 'hole', 'duplicate', 'outside', 'path'):
            schema = copy.deepcopy(self.schema)
            if mutation == 'overlap': schema['channels'][1]['offset'] = schema['channels'][0]['offset']
            elif mutation == 'hole': schema['channels'].pop()
            elif mutation == 'duplicate': schema['channels'][1]['name'] = schema['channels'][0]['name']
            elif mutation == 'outside': schema['channels'][-1]['offset'] = 31
            else: schema['ini_file'] = '../outside.ini'
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                render(schema, self.template, self.fragments)

    def test_rejects_unknown_gauges(self):
        self.fragments['front_page'] = 'gauge1 = unknownGauge\n'
        with self.assertRaises(ValueError): render(self.schema, self.template, self.fragments)


if __name__ == '__main__': unittest.main()
