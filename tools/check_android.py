#!/usr/bin/env python3
"""Cross-check the Android identity client against actual C bench wire responses."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


def main():
    android = ROOT / "android/app/src"
    namespace = "{http://schemas.android.com/apk/res/android}"
    manifest = ET.parse(android / "main/AndroidManifest.xml").getroot()
    assert not manifest.findall("uses-permission"), "Offline Android app must not request network or storage permissions"
    feature = manifest.find("uses-feature")
    assert feature.get(namespace + "name") == "android.hardware.usb.host"
    filters = ET.parse(android / "main/res/xml/device_filter.xml").getroot()
    assert {(int(d.get("vendor-id")), int(d.get("product-id"))) for d in filters} == {(0xcafe, 0x4001), (0x0483, 0xdf11)}
    with tempfile.TemporaryDirectory(prefix="tunuino-android-") as directory:
        work = Path(directory)
        exe = work / ("bench.exe" if os.name == "nt" else "bench")
        subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
            "-I" + str(ROOT / "firmware/include"), *map(str, sorted((ROOT / "firmware/src").glob("*.c"))),
            str(ROOT / "unit_tests/test_tunerstudio.c"), "-lm", "-o", str(exe)], check=True)
        lines = subprocess.check_output([str(exe)], text=True, timeout=15).splitlines()
        vector = next(line.split()[1] for line in lines if line.startswith("native "))
        javac = ["javac"] if shutil.which("javac") else ["java", "--module", "jdk.compiler/com.sun.tools.javac.Main"]
        subprocess.run(javac + ["--release", "8", "-encoding", "UTF-8", "-d", str(work),
            str(android / "main/java/io/tunuino/android/BenchProtocol.java"),
            str(android / "test/java/io/tunuino/android/BenchProtocolChecks.java")], check=True)
        subprocess.run(["java", "-cp", str(work), "io.tunuino.android.BenchProtocolChecks", vector], check=True, timeout=15)
    print("Android manifest/USB IDs verified. USB Host and lifecycle require Android/device validation.")


if __name__ == "__main__":
    main()
