#!/usr/bin/env python3
"""Compile and check the portable C/Java core without GUI dependencies."""
import ctypes
import os
from pathlib import Path
import random
import shutil
import subprocess
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[1]


def run(args):
    return subprocess.run(args, cwd=ROOT, check=True, text=True, capture_output=True).stdout.strip()


def main():
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
                   java_root / "datalog/CsvLogReader.java", ROOT / "console/src/test/java/io/tunuino/CoreChecks.java"]
        javac = ["javac"] if shutil.which("javac") else ["java", "--module", "jdk.compiler/com.sun.tools.javac.Main"]
        run(javac + ["-encoding", "UTF-8", "--release", "17", "-d", str(work)] + [str(p) for p in sources])
        print(run(["java", "-cp", str(work), "io.tunuino.CoreChecks", c_vector]))
    print("Portable core verified. JavaFX UI and hardware builds require separate validation.")


if __name__ == "__main__":
    main()
