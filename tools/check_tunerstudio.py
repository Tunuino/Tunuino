#!/usr/bin/env python3
"""Check the actual bench wire replies against the distributed INI, without an ECU."""
import configparser
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import zlib

from ci_package_kit import validate_tunerstudio
from ci_scope import scope

ROOT = Path(__file__).resolve().parents[1]
INI_NAME = "tunuino_proteus_v07_f427_usb_bench.ini"


def decode(packet):
    length = int.from_bytes(packet[:2], "big")
    body = packet[2:-4]
    assert len(packet) == length + 6 and length == len(body)
    assert zlib.crc32(body) == int.from_bytes(packet[-4:], "big")
    assert body[0] == 0
    return body[1:]


def main():
    ini = ROOT / "firmware/tunerstudio/generated" / INI_NAME
    parser = configparser.ConfigParser(interpolation=None, strict=False)
    parser.read_string("[General]\n" + ini.read_text(encoding="ascii"))
    signature = json.loads(parser["TunerStudio"]["signature"])
    constants = parser["Constants"]
    assert constants["messageEnvelopeFormat"] == "msEnvelope_1.0"
    assert constants["endianness"] == "big" and constants.getint("nPages") == 1
    assert constants.getint("pageSize") == 1
    assert constants.getint("baudRate") == 115200
    assert parser["ConstantExtensions"]["readOnly"] == "layoutRevision"
    for command in ("burnCommand", "pageValueWrite", "pageChunkWrite"):
        assert json.loads(constants[command]) == ""
    assert json.loads(parser["TunerStudio"]["queryCommand"]) == "S"
    assert json.loads(parser["TunerStudio"]["versionInfo"]) == "V"
    assert json.loads(constants["pageReadCommand"]) == "R"
    channels = parser["OutputChannels"]
    assert json.loads(channels["ochGetCommand"]) == "A"
    assert channels.getint("ochBlockSize") == 31
    with tempfile.TemporaryDirectory(prefix="tunuino-ts-") as directory:
        work = Path(directory)
        exe = work / ("ts-check.exe" if os.name == "nt" else "ts-check")
        sources = sorted((ROOT / "firmware/src").glob("*.c"))
        subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                        "-I" + str(ROOT / "firmware/include"), *map(str, sources),
                        str(ROOT / "unit_tests/test_tunerstudio.c"), "-lm", "-o", str(exe)], check=True)
        lines = subprocess.check_output([str(exe)], text=True, timeout=15).splitlines()
        assert lines[-1] == "OK"
        vectors = {name: bytes.fromhex(value) for name, value in (line.split() for line in lines[:-1])}
        assert vectors["plain_f"] == b"001"
        assert vectors["plain_q"] == vectors["plain_s"] == signature.encode("ascii") + b"\0"
        assert decode(vectors["s"]) == vectors["plain_q"]
        assert decode(vectors["v"]) == b"Tunuino 0.0.3 Proteus v0.7 USB bench\0"
        assert decode(vectors["r"]) == b"\x01"
        data = decode(vectors["a"])
        assert data == decode(vectors["native"])
        assert data == b"TUNUINO" + bytes([1,1,1,0,7,0,0,3]) + bytes(4) + bytes(range(1,13))
        assert len(data) == channels.getint("ochBlockSize")
        expected = {"protocolrevision": 1, "boardfamily": 1, "mcuseries": 1, "hardwarerevision": 7,
                    "firmwaremajor": 0, "firmwareminor": 0, "firmwarepatch": 3, "capabilities": 0,
                    "uidword1": 0x01020304, "uidword2": 0x05060708, "uidword3": 0x090A0B0C}
        widths = {"U08": 1, "U16": 2, "U32": 4}
        for name, value in expected.items():
            parts = next(csv.reader([channels[name]], skipinitialspace=True))
            assert parts[0] == "scalar" and int(parts[4]) == 1 and int(parts[5]) == 0
            offset, size = int(parts[2]), widths[parts[1]]
            assert offset + size <= len(data)
            assert int.from_bytes(data[offset:offset+size], "big") == value, name
        # Ensure every gauge resolves to an existing channel and front-page entry.
        gauges = parser["GaugeConfigurations"]
        for name, value in gauges.items():
            if name == "gaugecategory":
                continue
            assert value.split(",")[0].strip().lower() in expected
        for value in parser["FrontPage"].values():
            assert value.lower() in gauges
        # Exercise publication gates with a valid fixture, then a missing,
        # modified or wrong-signature INI. No network or physical hardware.
        (work / "firmware").mkdir()
        target = work / "firmware" / INI_NAME
        original = ini.read_bytes()
        target.write_bytes(original)
        manifest = {"artifacts_sha256": {INI_NAME: hashlib.sha256(original).hexdigest()},
                    "tunerstudio": {"ini_file": INI_NAME, "signature": signature,
                                    "output_bytes": 31, "read_only": True}}
        assert validate_tunerstudio(work, manifest) == manifest["tunerstudio"]
        for action in ("missing", "tampered", "mismatched"):
            target.write_bytes(original)
            if action == "missing": target.unlink()
            elif action == "tampered": target.write_bytes(original+b"; change\n")
            else: manifest["tunerstudio"]["signature"] = "wrong firmware"
            try:
                validate_tunerstudio(work, manifest)
            except (ValueError, FileNotFoundError):
                pass
            else:
                raise AssertionError("Publication accepted " + action + " INI")
    import sys
    subprocess.run([sys.executable, str(ROOT / "tools/check_io.py")], check=True, timeout=45)
    assert all(scope(["firmware/tunerstudio/generated/" + INI_NAME]).values())
    print("TunerStudio bench: INI/signature, independent CRC/channel offsets, fragmented and plain discovery, "
          "native coexistence, write rejection, TX deadlines and missing/mixed INI publication gates passed. No physical ECU.")


if __name__ == "__main__":
    main()
