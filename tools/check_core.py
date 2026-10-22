#!/usr/bin/env python3
"""Compile and check the portable C/Java core without GUI dependencies."""
import ctypes
import os
from pathlib import Path
import random
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[1]


def run(args, timeout=None):
    return subprocess.run(args, cwd=ROOT, check=True, text=True, capture_output=True, timeout=timeout).stdout.strip()


def main():
    run([sys.executable, "tools/check_kicad.py"], timeout=15)
    print("KiCad XML membership, ambiguity, provenance and file preservation checks passed")
    run([sys.executable, "tools/check_kicad_pcb.py"], timeout=15)
    print("KiCad PCB parser, pad membership, anonymous nodes and file preservation checks passed")
    run([sys.executable, "tools/check_kicad_components.py"], timeout=15)
    print("KiCad component/hierarchy comparison and MCU ambiguity checks passed")
    compiler = os.environ.get("CC", "cc")
    for command in (compiler, "java"):
        if not shutil.which(command):
            raise SystemExit(f"Required tool not found: {command}")
    with tempfile.TemporaryDirectory(prefix="tunuino-check-") as directory:
        work = Path(directory)
        executable = work / ("frame-check.exe" if os.name == "nt" else "frame-check")
        flags = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic", "-Ifirmware/include"]
        run(flags + ["firmware/src/frame.c", "firmware/tests/test_frame.c", "-o", str(executable)])
        c_vector = run([str(executable)])
        print("C CRC, corruption, capacity, boundary and overlap checks passed")
        crank_exe = work / ("crank-check.exe" if os.name == "nt" else "crank-check")
        run(flags + ["firmware/src/crank.c", "firmware/tests/test_crank.c", "-lm", "-o", str(crank_exe)])
        print(run([str(crank_exe)]))
        position_exe = work / ("position-check.exe" if os.name == "nt" else "position-check")
        run(flags + ["firmware/src/crank.c", "firmware/src/position.c", "firmware/tests/test_position.c",
                     "-lm", "-o", str(position_exe)])
        print(run([str(position_exe)]))
        capture_exe = work / ("capture-check.exe" if os.name == "nt" else "capture-check")
        thread_flags = [] if os.name == "nt" else ["-pthread"]
        run(flags + thread_flags + ["firmware/src/capture_queue.c", "firmware/src/crank.c", "firmware/src/position.c",
                     "firmware/tests/test_capture_queue.c", "-lm", "-o", str(capture_exe)])
        print(run([str(capture_exe)], timeout=15))
        identity_exe = work / ("identity-check.exe" if os.name == "nt" else "identity-check")
        run(flags + ["firmware/src/frame.c", "firmware/src/identity.c", "firmware/tests/test_identity.c", "-o", str(identity_exe)])
        identity_output = run([str(identity_exe)]).splitlines()
        identity_vector = identity_output[0]
        identity_body = bytes.fromhex(identity_vector)[2:-4]
        expected_identity = bytes([0]) + b"TUNUINO" + bytes([1, 1, 1, 1, 2, 0, 1, 3]) + bytes(4) + bytes(range(1, 13))
        if identity_body != expected_identity:
            raise AssertionError("C identity differs from independent wire layout")
        encoded_identity = bytes.fromhex(identity_vector)
        if zlib.crc32(identity_body) != int.from_bytes(encoded_identity[-4:], "big"):
            raise AssertionError("C identity CRC mismatch")
        print(identity_output[1])
        live_exe = work / ("live-check.exe" if os.name == "nt" else "live-check")
        run(flags + ["firmware/src/frame.c", "firmware/src/live_data.c", "firmware/tests/test_live_data.c", "-o", str(live_exe)])
        live_output = run([str(live_exe)]).splitlines()
        live_vector = live_output[0]
        live_packet = bytes.fromhex(live_vector)
        expected_live = struct.pack(">BBBBQIIIi", 0, 1, 15, 0, 0x123456789AB, 950125, 98765, 1023456, -12345)
        if live_packet[2:-4] != expected_live or zlib.crc32(expected_live) != int.from_bytes(live_packet[-4:], "big"):
            raise AssertionError("C live data differs from independent wire layout")
        print(live_output[1])
        map_exe = work / ("map-check.exe" if os.name == "nt" else "map-check")
        run(flags + ["firmware/src/frame.c", "firmware/src/calibration.c", "firmware/tests/test_calibration.c", "-lm", "-o", str(map_exe)])
        map_output = run([str(map_exe)]).splitlines()
        map_vector, ignition_vector = map_output[0:2]
        map_packet = bytes.fromhex(map_vector)
        expected_map = b"TUNUINOM" + bytes([1, 1, 2, 2]) + struct.pack(">8i", 1000, 3000, 50000, 100000, 40000, 60000, 80000, 100000)
        if map_packet[:-4] != expected_map or zlib.crc32(expected_map) != int.from_bytes(map_packet[-4:], "big"):
            raise AssertionError("C map differs from independent file layout")
        print(map_output[2])
        read_exe = work / ("map-read-check.exe" if os.name == "nt" else "map-read-check")
        run(flags + ["firmware/src/frame.c", "firmware/src/calibration.c", "firmware/src/map_read.c",
                     "firmware/tests/test_map_read.c", "-lm", "-o", str(read_exe)])
        read_output = run([str(read_exe)]).splitlines()
        for kind in (1, 2):
            packets, file = (bytes.fromhex(v) for v in read_output[(kind - 1) * 2:kind * 2])
            if len(file) != 4368 or file[:12] != b"TUNUINOM" + bytes([1, kind, 32, 32]):
                raise AssertionError("Unexpected maximum map fixture")
            cursor, offset = 0, 0
            while cursor < len(packets):
                length = int.from_bytes(packets[cursor:cursor + 2], "big")
                body = packets[cursor + 2:cursor + 2 + length]
                crc = int.from_bytes(packets[cursor + 2 + length:cursor + 6 + length], "big")
                if zlib.crc32(body) != crc:
                    raise AssertionError("Map transfer frame CRC differs from Python")
                if cursor == 0:
                    expected = struct.pack(">BBBBIHI", 0, 1, 0, kind, 0xFEDCBA98, len(file), zlib.crc32(file))
                else:
                    count = min(256, len(file) - offset)
                    expected = struct.pack(">BBBBIHH", 0, 1, 1, kind, 0xFEDCBA98, offset, count) + file[offset:offset + count]
                    offset += count
                if body != expected:
                    raise AssertionError("C map transfer differs from independent wire layout")
                cursor += length + 6
            if cursor != len(packets) or offset != len(file):
                raise AssertionError("Incomplete map fixture")
        print(read_output[4])
        dispatch_exe = work / ("dispatch-check.exe" if os.name == "nt" else "dispatch-check")
        run(flags + ["firmware/src/frame.c", "firmware/src/identity.c", "firmware/src/live_data.c",
                     "firmware/src/calibration.c", "firmware/src/map_read.c", "firmware/src/dispatch.c",
                     "firmware/tests/test_dispatch.c", "-lm", "-o", str(dispatch_exe)])
        print(run([str(dispatch_exe)]))
        rx_exe = work / ("rx-check.exe" if os.name == "nt" else "rx-check")
        run(flags + ["firmware/src/frame.c", "firmware/src/identity.c", "firmware/src/live_data.c",
                     "firmware/src/calibration.c", "firmware/src/map_read.c", "firmware/src/dispatch.c",
                     "firmware/src/stream_rx.c", "firmware/tests/test_stream_rx.c", "-lm", "-o", str(rx_exe)])
        print(run([str(rx_exe)]))
        tx_exe = work / ("tx-check.exe" if os.name == "nt" else "tx-check")
        run(flags + ["firmware/src/frame.c", "firmware/src/identity.c", "firmware/src/live_data.c",
                     "firmware/src/calibration.c", "firmware/src/map_read.c", "firmware/src/dispatch.c",
                     "firmware/src/stream_rx.c", "firmware/src/stream_tx.c",
                     "firmware/tests/test_stream_tx.c", "-lm", "-o", str(tx_exe)])
        print(run([str(tx_exe)]))
        link_exe = work / ("link-check.exe" if os.name == "nt" else "link-check")
        run(flags + ["firmware/src/frame.c", "firmware/src/identity.c", "firmware/src/live_data.c",
                     "firmware/src/calibration.c", "firmware/src/map_read.c", "firmware/src/dispatch.c",
                     "firmware/src/stream_rx.c", "firmware/src/stream_tx.c", "firmware/src/link.c",
                     "firmware/tests/test_link.c", "-lm", "-o", str(link_exe)])
        print(run([str(link_exe)]))
        peer_exe = work / ("link-peer.exe" if os.name == "nt" else "link-peer")
        run(flags + ["firmware/src/frame.c", "firmware/src/identity.c", "firmware/src/live_data.c",
                     "firmware/src/calibration.c", "firmware/src/map_read.c", "firmware/src/dispatch.c",
                     "firmware/src/stream_rx.c", "firmware/src/stream_tx.c", "firmware/src/link.c",
                     "firmware/tests/link_peer.c", "-lm", "-o", str(peer_exe)])
        library_path = work / ("frame.dll" if os.name == "nt" else "frame.so")
        shared_flags = ["-shared"] + ([] if os.name == "nt" else ["-fPIC"])
        run(flags + shared_flags + ["firmware/src/frame.c", "-o", str(library_path)])
        library = ctypes.CDLL(str(library_path))
        byte_ptr = ctypes.POINTER(ctypes.c_uint8)
        library.tunuino_frame_decode.argtypes = [byte_ptr, ctypes.c_size_t, ctypes.POINTER(byte_ptr), ctypes.POINTER(ctypes.c_size_t)]
        library.tunuino_frame_decode.restype = ctypes.c_int
        rng = random.Random(20261021)
        for count in [1, 255, 256, 4096] + [rng.randrange(1, 4097) for _ in range(100)]:
            body = bytes(rng.randrange(256) for _ in range(count))
            packet = count.to_bytes(2, "big") + body + zlib.crc32(body).to_bytes(4, "big")
            buffer = (ctypes.c_uint8 * len(packet)).from_buffer_copy(packet)
            decoded = byte_ptr()
            length = ctypes.c_size_t()
            result = library.tunuino_frame_decode(buffer, len(packet), ctypes.byref(decoded), ctypes.byref(length))
            if result != 0 or length.value != count or ctypes.string_at(decoded, count) != body:
                raise AssertionError("C decoder differs from independent CRC32 vectors")
        print("104 independent Python-to-C packet vectors passed")
        # Releasing the DLL before temporary-directory cleanup matters on Windows.
        if os.name == "nt":
            import _ctypes
            _ctypes.FreeLibrary(library._handle)
        java_root = ROOT / "console/src/main/java/io/tunuino"
        sources = [java_root / "protocol/FrameCodec.java", java_root / "datalog/LogSample.java",
                   java_root / "datalog/CsvLogReader.java", java_root / "transport/SerialChannel.java",
                   java_root / "transport/PacketSession.java",
                   java_root / "protocol/DeviceIdentity.java", java_root / "protocol/IdentityClient.java",
                   java_root / "protocol/MapReadClient.java",
                   java_root / "protocol/LiveDataClient.java", java_root / "datalog/CsvLogWriter.java",
                   java_root / "datalog/LiveDataRecorder.java",
                   java_root / "calibration/CalibrationMap.java",
                   java_root / "calibration/CalibrationDocument.java",
                   ROOT / "console/src/test/java/io/tunuino/CoreChecks.java",
                   ROOT / "console/src/test/java/io/tunuino/TransportChecks.java",
                   ROOT / "console/src/test/java/io/tunuino/IdentityChecks.java",
                   ROOT / "console/src/test/java/io/tunuino/LiveDataChecks.java",
                   ROOT / "console/src/test/java/io/tunuino/CalibrationChecks.java",
                   ROOT / "console/src/test/java/io/tunuino/MapReadChecks.java",
                   ROOT / "console/src/test/java/io/tunuino/FirmwareLinkChecks.java"]
        javac = ["javac"] if shutil.which("javac") else ["java", "--module", "jdk.compiler/com.sun.tools.javac.Main"]
        run(javac + ["-encoding", "UTF-8", "--release", "17", "-d", str(work)] + [str(p) for p in sources])
        print(run(["java", "-cp", str(work), "io.tunuino.CoreChecks", c_vector]))
        print(run(["java", "-cp", str(work), "io.tunuino.TransportChecks"]))
        print(run(["java", "-cp", str(work), "io.tunuino.IdentityChecks", identity_vector]))
        print(run(["java", "-cp", str(work), "io.tunuino.LiveDataChecks", live_vector]))
        print(run(["java", "-cp", str(work), "io.tunuino.CalibrationChecks", map_vector, ignition_vector]))
        read_fixture = work / "map-read-vectors.txt"
        read_fixture.write_text("\n".join(read_output[:4]), encoding="ascii")
        print(run(["java", "-cp", str(work), "io.tunuino.MapReadChecks", str(read_fixture)]))
        print(run(["java", "-cp", str(work), "io.tunuino.FirmwareLinkChecks", str(peer_exe)], timeout=30))
    print("Portable core verified. JavaFX UI and hardware builds require separate validation.")


if __name__ == "__main__":
    main()
