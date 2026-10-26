"""Reject mismatched/tampered installers and preserve complete synchronized payloads."""
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from ci_build_installer import package_version, prepare_image, SETUP_NAME
from ci_release import validate_installer
import ci_package_kit
import ci_release


class InstallerChecks(unittest.TestCase):
    def test_cross_workflow_release_uses_source_and_kit_identity(self):
        # GitHub's reserved IDs describe the publisher, not its upstream kit run.
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); kit = root / 'build/kit'; installer = root / 'build/installer'
            (kit / 'console/app').mkdir(parents=True)
            (kit / 'console/tools').mkdir()
            (kit / 'firmware').mkdir()
            installer.mkdir()
            source = 'a' * 40
            jar = b'Console JAR fixture'; firmware = b'Firmware fixture'; msi = b'MSI fixture'
            (kit / 'console/app/tunuino-console.jar').write_bytes(jar)
            (kit / 'console/TunuinoConsole.exe').write_bytes(b'launcher fixture')
            (kit / 'console/tools/tools-manifest.json').write_text(json.dumps({'bundled': {}}))
            (kit / 'console/package-manifest.json').write_text(json.dumps({
                'source_commit': source, 'main_jar_sha256': hashlib.sha256(jar).hexdigest()}))
            (kit / 'firmware/test.bin').write_bytes(firmware)
            (kit / 'firmware/test.bin.properties').write_text('fixture')
            (kit / 'firmware/manifest.json').write_text(json.dumps({
                'source_commit': source, 'artifacts_sha256': {'test.bin': hashlib.sha256(firmware).hexdigest()}}))
            (installer / SETUP_NAME).write_bytes(msi)
            (installer / 'installer-manifest.json').write_text(json.dumps({
                'source_commit': source, 'run_id': '123', 'file': SETUP_NAME,
                'sha256': hashlib.sha256(msi).hexdigest(), 'install_upgrade_uninstall_verified': True,
                'user_data_preserved': True, 'installed_runtime_verified': True}))
            previous = Path.cwd()
            try:
                os.chdir(root)
                with patch.dict(os.environ, {'SOURCE_SHA': source, 'KIT_RUN_ID': '123',
                                             'GITHUB_SHA': 'b' * 40, 'GITHUB_RUN_ID': '456',
                                             'GITHUB_REPOSITORY': 'Tunuino/Tunuino'}), \
                        patch.object(ci_package_kit, 'validate_tunerstudio', return_value={}), \
                        patch.object(ci_release, 'validate_tunerstudio', return_value={}):
                    ci_package_kit.main()
                    manifest = json.loads((kit / 'kit-manifest.json').read_text())
                    self.assertEqual(manifest['source_commit'], source)
                    self.assertEqual(manifest['run_id'], '123')
                    ci_release.main()
                self.assertEqual((root / 'build/release' / SETUP_NAME).read_bytes(), msi)
                self.assertTrue((root / 'build/release/tunuino_bundle_proteus_v07_f427_usb_bench.zip').is_file())
                notes = (root / 'build/release-notes.md').read_text()
                self.assertIn('/actions/runs/123', notes)
                self.assertIn('/commit/' + source, notes)
            finally:
                os.chdir(previous)

    def test_versions_sort_and_obey_msi_limits(self):
        self.assertEqual(package_version(65535), '1.0.65535')
        self.assertEqual(package_version(65536), '1.1.0')
        with self.assertRaises(ValueError): package_version(256 * 65536)
        with self.assertRaises(ValueError): package_version(-1)

    def test_complete_payload_with_provenance(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); kit = root / 'kit'; image = root / 'image'
            (kit / 'console/app').mkdir(parents=True)
            (kit / 'console/app/tunuino-console.jar').write_bytes(b'jar')
            (kit / 'console/app/.jpackage.xml').write_text('<jpackage-state/>')
            (kit / 'firmware').mkdir()
            for name in ('test.bin', 'test.bin.properties', 'test.ini', 'corresponding-source.tar.gz', 'LICENSE'):
                (kit / 'firmware' / name).write_bytes(name.encode())
            (kit / 'kit-manifest.json').write_text(json.dumps({'source_commit': 'a' * 40, 'run_id': '123'}))
            with self.assertRaises(ValueError): prepare_image(kit, image, 'b' * 40)
            self.assertFalse(image.exists())
            prepare_image(kit, image, 'a' * 40)
            manifest = json.loads((image / 'installation-manifest.json').read_text())
            for name, digest in manifest['files_sha256'].items():
                self.assertEqual(hashlib.sha256((image / name).read_bytes()).hexdigest(), digest)
            self.assertTrue((image / 'firmware/test.bin.properties').is_file())
            self.assertTrue((image / 'firmware/test.ini').is_file())
            self.assertTrue((image / 'firmware/corresponding-source.tar.gz').is_file())
            self.assertTrue((image / 'app/.jpackage.xml').is_file())
            self.assertNotIn('app/.jpackage.xml', manifest['files_sha256'])
            self.assertFalse((image / 'maps').exists())

    def test_release_rejects_unverified_mixed_or_changed_installer(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); package = root / SETUP_NAME
            package.write_bytes(b'MSI fixture')
            metadata = {'source_commit': 'a' * 40, 'run_id': '123', 'file': SETUP_NAME,
                        'sha256': hashlib.sha256(package.read_bytes()).hexdigest(),
                        'install_upgrade_uninstall_verified': True, 'user_data_preserved': True,
                        'installed_runtime_verified': True}
            def write(): (root / 'installer-manifest.json').write_text(json.dumps(metadata))
            write()
            self.assertEqual(validate_installer(root, 'a' * 40, '123'), package)
            with self.assertRaises(ValueError): validate_installer(root, 'b' * 40, '123')
            with self.assertRaises(ValueError): validate_installer(root, 'a' * 40, '124')
            for check in ('install_upgrade_uninstall_verified', 'user_data_preserved', 'installed_runtime_verified'):
                metadata[check] = False; write()
                with self.assertRaises(ValueError): validate_installer(root, 'a' * 40, '123')
                metadata[check] = True
            write(); package.write_bytes(b'Changed after validation')
            with self.assertRaises(ValueError): validate_installer(root, 'a' * 40, '123')


if __name__ == '__main__': unittest.main()
