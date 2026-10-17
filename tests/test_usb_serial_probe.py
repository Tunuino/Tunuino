"""Tests for read-only USB CDC signature probing."""
import importlib.util
from pathlib import Path
import unittest

MODULE = Path(__file__).resolve().parents[1] / "firmware" / "console" / "usb_serial_probe.py"
spec = importlib.util.spec_from_file_location("usb_serial_probe", MODULE)
probe_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe_module)

class FakeSerial:
    def __init__(self, response=b"Tunuino-TS-0.1", written=1, **kwargs):
        self.response = response
        self.written = written
        self.sent = b""
    def __enter__(self):
        return self
    def __exit__(self, *args):
        return False
    def reset_input_buffer(self):
        pass
    def write(self, data):
        self.sent = data
        return self.written
    def read(self, count):
        assert count == 96
        return self.response

class ProbeTests(unittest.TestCase):
    def test_tunuino_identity(self):
        link = FakeSerial()
        self.assertEqual(probe_module.probe("COM4", serial_factory=lambda **kw: link),
                         "Tunuino-TS-0.1")
        self.assertEqual(link.sent, b"S")
    def test_external_identity(self):
        self.assertEqual(probe_module.probe("/dev/ttyACM0",
                         serial_factory=lambda **kw: FakeSerial(b"speeduino")),
                         "speeduino")
    def test_reject_empty_and_binary(self):
        for response in (b"", b"ECU\x00invalid"):
            with self.subTest(response=response), self.assertRaises(ValueError):
                probe_module.probe("COM4", serial_factory=lambda **kw: FakeSerial(response))
    def test_reject_short_write(self):
        with self.assertRaises(IOError):
            probe_module.probe("COM4", serial_factory=lambda **kw: FakeSerial(written=0))
    def test_reject_invalid_timeout(self):
        with self.assertRaises(ValueError):
            probe_module.probe("COM4", timeout=0, serial_factory=FakeSerial)

if __name__ == "__main__":
    unittest.main()
