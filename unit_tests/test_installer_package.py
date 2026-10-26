"""Reject mismatched/tampered installers and preserve complete synchronized payloads."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from ci_build_installer import package_version, prepare_image, SETUP_NAME
from ci_release import validate_installer


class InstallerChecks(unittest.TestCase):
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
