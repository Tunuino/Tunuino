"""Shipping helpers must produce fresh matching kits; desktop/tests do not flash-build."""
from pathlib import Path
import runpy
import unittest

scope = runpy.run_path(str(Path(__file__).resolve().parents[1] / 'tools/ci_scope.py'))['scope']


class ScopeChecks(unittest.TestCase):
    def test_shipping_changes_rebuild_both_components(self):
        for path in ('java_console/src/main/java/io/tunuino/console/TunuinoConsole.java',
                     'firmware/boards/proteus_v07/chibios_usb_bench/main.c',
                     'misc/install/package_programmer_tools.py'):
            result = scope([path])
            with self.subTest(path=path):
                self.assertTrue(result['core'] and result['bench'] and result['console'])

    def test_desktop_and_test_changes_stay_scoped(self):
        for path in ('unit_tests/test_frame.c', 'simulator/model.c',
                     'java_tools/src/main/java/io/tunuino/tools/Tools.java',
                     'java_console/src/test/java/io/tunuino/CoreChecks.java'):
            result = scope([path])
            with self.subTest(path=path):
                self.assertTrue(result['core'])
                self.assertFalse(result['arm'] or result['bench'] or result['console'])

    def test_definition_changes_validate_every_component(self):
        self.assertTrue(all(scope(['firmware/tunerstudio/schema.json']).values()))
        self.assertFalse(any(scope(['docs/architecture.md']).values()))


if __name__ == '__main__': unittest.main()
