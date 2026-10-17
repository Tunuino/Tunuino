"""Host tests for the STM32F429 SWD flashing entry point."""
import tempfile
import hashlib
import unittest
from pathlib import Path
from unittest.mock import patch
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "firmware" / "console"))
import flash_stm32


class FlashTests(unittest.TestCase):
    def test_dry_run_and_confirmation(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "firmware.bin"
            path.write_bytes((0x20020000).to_bytes(4, "little") + (0x08000101).to_bytes(4, "little") + bytes(128))
            sha = hashlib.sha256(path.read_bytes()).hexdigest()
            with self.assertRaises(ValueError):
                flash_stm32.flash(path, "STM32_Programmer_CLI", False, True, sha)
            with self.assertRaises(ValueError):
                flash_stm32.flash(path, "STM32_Programmer_CLI", True, True, "0" * 64)
            command = flash_stm32.flash(path, "STM32_Programmer_CLI", True, True, sha)
            self.assertIn("-v", command)
            self.assertIn("port=SWD", command)
            with patch.object(flash_stm32.shutil, "which", return_value="/usr/bin/stm32cli"):
                with patch.object(flash_stm32.subprocess, "run") as run:
                    with patch.object(flash_stm32.subprocess, "run", side_effect=[__import__("subprocess").CompletedProcess([], 0, "Device ID : 0x419", ""), __import__("subprocess").CompletedProcess([], 0)]) as run:
                        flash_stm32.flash(path, "STM32_Programmer_CLI", True, False, sha, "0x419")
                    self.assertEqual(run.call_count, 2)
                    self.assertTrue(run.call_args.kwargs["check"])

    def test_bad_vector_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "firmware.bin"
            path.write_bytes(bytes(128))
            with self.assertRaises(ValueError):
                flash_stm32.validate_image(path)


if __name__ == "__main__":
    unittest.main()
