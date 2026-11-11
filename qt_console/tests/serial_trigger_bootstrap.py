#!/usr/bin/env python3
"""POSIX host integration: real Qt serial/Console and the native firmware dispatcher.
Run after building CompleteMapSmoke and MapWriteFirmwareHarness. No physical ECU.
"""
import argparse
import os
from pathlib import Path
import pty
import select
import subprocess
import tempfile
import time
import tty
import zlib


def check_case(console, firmware, fault_page):
    master, slave = pty.openpty()
    tty.setraw(slave)
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen", TUNUINO_SERIAL_TEST_PORT=os.ttyname(slave),
               TUNUINO_SERIAL_FAULT=str(fault_page) if fault_page else "")
    trace = []
    fw = subprocess.Popen([firmware, "--empty-advanced", "--usb-only"], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    try:
        with tempfile.TemporaryFile(mode="w+") as log:
            app = subprocess.Popen([console], env=env, stdout=log, stderr=log)
            try:
                incoming = b""
                deadline = time.monotonic() + 25
                while app.poll() is None and time.monotonic() < deadline:
                    if not select.select([master], [], [], .1)[0]:
                        continue
                    incoming += os.read(master, 4096)
                    while len(incoming) >= 2:
                        size = int.from_bytes(incoming[:2], "big")
                        assert 0 < size < 4096, "Invalid Console frame"
                        if len(incoming) < size + 6:
                            break
                        body = incoming[2:2 + size]
                        assert zlib.crc32(body) == int.from_bytes(incoming[2 + size:6 + size], "big")
                        incoming = incoming[size + 6:]
                        assert body[:1] not in (b"w", b"b"), "Unexpected configuration write/Burn"
                        trace.append(body)
                        fw.stdin.write(body.hex().encode() + b"\n")
                        fw.stdin.flush()
                        reply = bytes.fromhex(fw.stdout.readline().decode().strip())
                        if fault_page and body[:1] == b"r" and int.from_bytes(body[1:3], "big") == fault_page and body[3:5] == b"\0\0":
                            # Fault in the read response only; native stored data is untouched.
                            reply = reply[:1] + b"\xff\xff" + reply[3:]
                        os.write(master, len(reply).to_bytes(2, "big") + reply + zlib.crc32(reply).to_bytes(4, "big"))
                if app.poll() is None:
                    app.kill()
                    app.wait()
                    raise AssertionError("Console integration timed out")
                log.seek(0)
                output = log.read()
                assert app.returncode == 0, output
                assert any(body.startswith(b"TUNUINO:S") for body in trace), output
                pages = [int.from_bytes(body[1:3], "big") for body in trace if body[:1] == b"r"]
                assert pages[:8] == [1] * 4 + [4] * 4, pages
                print(f"USB-only bootstrap: profile absent, fault page {fault_page or 'none'}, Scope with no engine sync: passed")
            finally:
                if app.poll() is None:
                    app.kill()
                    app.wait()
    finally:
        fw.stdin.close()
        fw.wait(timeout=5)
        os.close(master)
        os.close(slave)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--console", required=True, type=lambda p: str(Path(p).resolve()))
    parser.add_argument("--firmware", required=True, type=lambda p: str(Path(p).resolve()))
    args = parser.parse_args()
    for failure in (0, 2, 8):
        check_case(args.console, args.firmware, failure)
