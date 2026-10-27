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
from build_simulator import build as build_simulator

ROOT = Path(__file__).resolve().parents[1]


def run(args, timeout=None):
    if any(p in args for p in ("firmware/src/engine_control.c","firmware/src/aux_control.c","firmware/src/device.c","firmware/src/dispatch.c")):
        args=list(args)
        if "firmware/src/engine_features.c" not in args: args.append("firmware/src/engine_features.c")
    if "firmware/src/dispatch.c" in args:
        args = list(args)
        for source in ("firmware/src/device.c", "firmware/src/config_journal.c", "firmware/src/analog.c",
                       "firmware/src/calibration.c", "firmware/src/engine_math.c", "firmware/src/engine_profile.c", "firmware/src/fuel_corrections.c", "firmware/src/aux_control.c"):
            if source not in args: args.append(source)
    result = subprocess.run(args, cwd=ROOT, text=True, capture_output=True, timeout=timeout)
    if result.returncode:
        raise RuntimeError('Check failed:\n' + (result.stdout + result.stderr)[-12000:])
    return result.stdout.strip()


def main():
    print(run([sys.executable, "misc/actions/verify_generated.py"], timeout=15))
    run([sys.executable, "-m", "unittest", "discover", "-s", "unit_tests", "-p", "test_*.py"], timeout=15)
    print(run([sys.executable, "tools/check_simulator.py"], timeout=30))
    print(run([sys.executable, "tools/check_android.py"], timeout=60))
    print(run([sys.executable, "tools/check_tunerstudio.py"], timeout=60))
    print(run([sys.executable, "tools/check_configuration.py"], timeout=30))
    print(run([sys.executable, "tools/check_maintenance.py"], timeout=60))
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
        output_exe=work / ("outputs-check.exe" if os.name=="nt" else "outputs-check")
        run(flags+["firmware/src/outputs.c","firmware/src/engine_control.c","firmware/src/engine_profile.c","firmware/src/fuel_corrections.c","unit_tests/test_outputs.c","-lm","-o",str(output_exe)])
        print(run([str(output_exe)]))
        features_exe=work / ("features-check.exe" if os.name=="nt" else "features-check")
        run(flags+["firmware/src/engine_features.c","unit_tests/test_engine_features.c","-o",str(features_exe)])
        print(run([str(features_exe)]))
        for name,sources in [("aux-control",["firmware/src/aux_control.c","unit_tests/test_aux_control.c"]),("pwm",["firmware/src/outputs.c","unit_tests/test_pwm.c"])]:
            extra=work / (name+(".exe" if os.name=="nt" else ""));run(flags+sources+["-o",str(extra)]);print(run([str(extra)]))
        fuel_exe=work / ("fuel-corrections.exe" if os.name=="nt" else "fuel-corrections")
        run(flags+["firmware/src/fuel_corrections.c","unit_tests/test_fuel_corrections.c","-o",str(fuel_exe)])
        print(run([str(fuel_exe)]))
        mailbox_exe = work / ("mailbox-check.exe" if os.name == "nt" else "mailbox-check")
        mailbox_threads = [] if os.name == "nt" else ["-pthread"]
        run(flags + mailbox_threads + ["firmware/src/live_mailbox.c", "unit_tests/test_live_mailbox.c",
                                      "-o", str(mailbox_exe)])
        print(run([str(mailbox_exe)], timeout=15))
        run(flags + ["firmware/src/frame.c", "unit_tests/test_frame.c", "-o", str(executable)])
        c_vector = run([str(executable)])
        print("C CRC, corruption, capacity, boundary and overlap checks passed")
        engine_exe = work / ("engine-math-check.exe" if os.name == "nt" else "engine-math-check")
        run(flags + ["firmware/src/engine_math.c", "unit_tests/test_engine_math.c",
                     "-o", str(engine_exe)])
        print(run([str(engine_exe)]))
        analog_exe = work / ("analog-check.exe" if os.name == "nt" else "analog-check")
        run(flags + ["firmware/src/analog.c", "unit_tests/test_analog.c", "-o", str(analog_exe)])
        print(run([str(analog_exe)]))
        clock_exe = work / ("clock-check.exe" if os.name == "nt" else "clock-check")
        run(flags + ["firmware/src/clock32.c", "firmware/src/capture_queue.c", "firmware/src/crank.c",
                     "firmware/src/position.c", "firmware/src/capture_processor.c",
                     "unit_tests/test_clock32.c", "-lm", "-o", str(clock_exe)])
        print(run([str(clock_exe)]))
        source_exe = work / ("source-check.exe" if os.name == "nt" else "source-check")
        run(flags + ["firmware/src/clock32.c", "firmware/src/capture_source.c", "firmware/src/capture_queue.c",
                     "firmware/src/crank.c", "firmware/src/position.c", "firmware/src/capture_processor.c",
                     "firmware/src/telemetry.c", "firmware/src/live_data.c", "firmware/src/frame.c",
                     "unit_tests/test_capture_source.c", "-lm", "-o", str(source_exe)])
        print(run([str(source_exe)]))
        crank_exe = work / ("crank-check.exe" if os.name == "nt" else "crank-check")
        run(flags + ["firmware/src/crank.c", "unit_tests/test_crank.c", "-lm", "-o", str(crank_exe)])
        print(run([str(crank_exe)]))
        position_exe = work / ("position-check.exe" if os.name == "nt" else "position-check")
        run(flags + ["firmware/src/crank.c", "firmware/src/position.c", "unit_tests/test_position.c",
                     "-lm", "-o", str(position_exe)])
        print(run([str(position_exe)]))
        capture_exe = work / ("capture-check.exe" if os.name == "nt" else "capture-check")
        thread_flags = [] if os.name == "nt" else ["-pthread"]
        run(flags + thread_flags + ["firmware/src/capture_queue.c", "firmware/src/crank.c", "firmware/src/position.c",
                     "unit_tests/test_capture_queue.c", "-lm", "-o", str(capture_exe)])
        print(run([str(capture_exe)], timeout=15))
        processor_exe = work / ("processor-check.exe" if os.name == "nt" else "processor-check")
        run(flags + ["firmware/src/capture_queue.c", "firmware/src/crank.c", "firmware/src/position.c",
                     "firmware/src/capture_processor.c", "unit_tests/test_capture_processor.c", "-lm", "-o", str(processor_exe)])
        print(run([str(processor_exe)], timeout=15))
        telemetry_exe = work / ("telemetry-check.exe" if os.name == "nt" else "telemetry-check")
        run(flags + ["firmware/src/frame.c", "firmware/src/live_data.c", "firmware/src/crank.c", "firmware/src/position.c",
                     "firmware/src/capture_queue.c", "firmware/src/capture_processor.c", "firmware/src/telemetry.c",
                     "unit_tests/test_telemetry.c", "-lm", "-o", str(telemetry_exe)])
        print(run([str(telemetry_exe)]))
        identity_exe = work / ("identity-check.exe" if os.name == "nt" else "identity-check")
        run(flags + ["firmware/src/frame.c", "firmware/src/identity.c", "unit_tests/test_identity.c", "-o", str(identity_exe)])
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
        run(flags + ["firmware/src/frame.c", "firmware/src/live_data.c", "unit_tests/test_live_data.c", "-o", str(live_exe)])
        live_output = run([str(live_exe)]).splitlines()
        live_vector = live_output[0]
        live_packet = bytes.fromhex(live_vector)
        expected_live = struct.pack(">BBBBQIIIi", 0, 1, 15, 0, 0x123456789AB, 950125, 98765, 1023456, -12345)
        if live_packet[2:-4] != expected_live or zlib.crc32(expected_live) != int.from_bytes(live_packet[-4:], "big"):
            raise AssertionError("C live data differs from independent wire layout")
        print(live_output[1])
        refined_exe = work / ("map-refine-check.exe" if os.name == "nt" else "map-refine-check")
        run(flags + ["firmware/src/frame.c", "firmware/src/calibration.c",
                     "unit_tests/test_map_refine.c", "-lm", "-o", str(refined_exe)])
        refined_output = run([str(refined_exe)]).splitlines()
        refined_file = work / "refined-16x16-vectors.txt"
        refined_file.write_text("\n".join(refined_output[:2]) + "\n", encoding="ascii")
        print(refined_output[-1])
        map_exe = work / ("map-check.exe" if os.name == "nt" else "map-check")
        run(flags + ["firmware/src/frame.c", "firmware/src/calibration.c", "unit_tests/test_calibration.c", "-lm", "-o", str(map_exe)])
        map_output = run([str(map_exe)]).splitlines()
        map_vector, ignition_vector = map_output[0:2]
        map_packet = bytes.fromhex(map_vector)
        expected_map = b"TUNUINOM" + bytes([1, 1, 2, 2]) + struct.pack(">8i", 1000, 3000, 50000, 100000, 40000, 60000, 80000, 100000)
        if map_packet[:-4] != expected_map or zlib.crc32(expected_map) != int.from_bytes(map_packet[-4:], "big"):
            raise AssertionError("C map differs from independent file layout")
        print(map_output[2])
        read_exe = work / ("map-read-check.exe" if os.name == "nt" else "map-read-check")
        run(flags + ["firmware/src/frame.c", "firmware/src/calibration.c", "firmware/src/map_read.c",
                     "unit_tests/test_map_read.c", "-lm", "-o", str(read_exe)])
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
                     "unit_tests/test_dispatch.c", "-lm", "-o", str(dispatch_exe)])
        print(run([str(dispatch_exe)]))
        rx_exe = work / ("rx-check.exe" if os.name == "nt" else "rx-check")
        run(flags + ["firmware/src/frame.c", "firmware/src/identity.c", "firmware/src/live_data.c",
                     "firmware/src/calibration.c", "firmware/src/map_read.c", "firmware/src/dispatch.c",
                     "firmware/src/stream_rx.c", "unit_tests/test_stream_rx.c", "-lm", "-o", str(rx_exe)])
        print(run([str(rx_exe)]))
        tx_exe = work / ("tx-check.exe" if os.name == "nt" else "tx-check")
        run(flags + ["firmware/src/frame.c", "firmware/src/identity.c", "firmware/src/live_data.c",
                     "firmware/src/calibration.c", "firmware/src/map_read.c", "firmware/src/dispatch.c",
                     "firmware/src/stream_rx.c", "firmware/src/stream_tx.c",
                     "unit_tests/test_stream_tx.c", "-lm", "-o", str(tx_exe)])
        print(run([str(tx_exe)]))
        link_exe = work / ("link-check.exe" if os.name == "nt" else "link-check")
        run(flags + ["firmware/src/frame.c", "firmware/src/identity.c", "firmware/src/live_data.c",
                     "firmware/src/calibration.c", "firmware/src/map_read.c", "firmware/src/dispatch.c",
                     "firmware/src/stream_rx.c", "firmware/src/stream_tx.c", "firmware/src/link.c",
                     "firmware/src/live_mailbox.c", "unit_tests/test_link.c", "-lm", "-o", str(link_exe)])
        print(run([str(link_exe)]))
        bench_object = work / "usb-bench-loop.o"
        vbus_exe = work / ("usb-vbus-check.exe" if os.name == "nt" else "usb-vbus-check")
        run(flags + ["-Iunit_tests/usb_port_stub", "-Ifirmware/boards/proteus_v07/usb_bench",
            "firmware/boards/proteus_v07/usb_bench/usb_port.c", "unit_tests/test_usb_vbus.c",
            "-o", str(vbus_exe)])
        print(run([str(vbus_exe)]))
        bench_include = ["-Iunit_tests/usb_bench_stub"]
        run(flags + bench_include + ["-Dmain=tunuino_usb_bench_main", "-c",
            "firmware/boards/proteus_v07/usb_bench/main.c", "-o", str(bench_object)])
        bench_exe = work / ("usb-bench-check.exe" if os.name == "nt" else "usb-bench-check")
        core_sources = [str(p.relative_to(ROOT)) for p in sorted((ROOT / "firmware/src").glob("*.c"))]
        migration=work / ("journal-migration.exe" if os.name=="nt" else "journal-migration")
        run(flags+core_sources+["unit_tests/test_journal_migration.c","-lm","-o",str(migration)]);print(run([str(migration)],timeout=30))
        output_board_exe=work / ("board-outputs-check.exe" if os.name=="nt" else "board-outputs-check")
        run(flags+["-Iunit_tests/output_board_stub","-Ifirmware/boards/proteus_v07/chibios_usb_bench"]
            +core_sources+["firmware/boards/proteus_v07/chibios_usb_bench/board_outputs.c",
                "unit_tests/test_board_outputs.c","-lm","-o",str(output_board_exe)])
        print(run([str(output_board_exe)]))
        output_peer=work / ("output-peer.exe" if os.name=="nt" else "output-peer")
        run(flags+["-Iunit_tests/output_board_stub","-Ifirmware/boards/proteus_v07/chibios_usb_bench","-DTUNUINO_OUTPUT_PEER"]
            +core_sources+["firmware/boards/proteus_v07/chibios_usb_bench/board_outputs.c",
                "unit_tests/test_board_outputs.c","-lm","-o",str(output_peer)])
        run(flags + bench_include + core_sources + ["unit_tests/test_usb_bench.c",
            str(bench_object), "-lm", "-o", str(bench_exe)])
        for mode in ("normal", "timeout", "corrupt", "close"):
            print(run([str(bench_exe), mode], timeout=10))
        io_exe = work / ("io-check.exe" if os.name == "nt" else "io-check")
        run(flags + core_sources + ["unit_tests/test_device_io.c", "-lm", "-o", str(io_exe)])
        assert run([str(io_exe)], timeout=20).splitlines()[-1] == "OK"
        print("IO configuration, calibration, partial telemetry, maps and flash interruption cases passed. Host model.")
        io_peer = work / ("io-peer.exe" if os.name == "nt" else "io-peer")
        run(flags + core_sources + ["unit_tests/device_peer.c", "-lm", "-o", str(io_peer)])
        chibios_object = work / "chibios-bench-loop.o"
        chibios_include = ["-Iunit_tests/chibios_bench_stub", "-Ifirmware/boards/proteus_v07/chibios_usb_bench"]
        status_source = "firmware/boards/proteus_v07/chibios_usb_bench/board_status.c"
        status_exe = work / ("status-check.exe" if os.name == "nt" else "status-check")
        run(flags + chibios_include + [status_source, "unit_tests/test_board_status.c", "-o", str(status_exe)])
        print(run([str(status_exe)], timeout=10))
        run(flags + chibios_include + ["-Dmain=tunuino_chibios_bench_main", "-c",
            "firmware/boards/proteus_v07/chibios_usb_bench/main.c", "-o", str(chibios_object)])
        chibios_exe = work / ("chibios-bench-check.exe" if os.name == "nt" else "chibios-bench-check")
        run(flags + chibios_include + core_sources + ["unit_tests/test_chibios_bench.c",
            status_source, str(chibios_object), "-lm", "-o", str(chibios_exe)])
        for mode in ("normal", "tunerstudio", "timeout", "corrupt", "disconnect", "reconfigure", "recover"):
            print(run([str(chibios_exe), mode], timeout=10))
        peer_exe = work / ("link-peer.exe" if os.name == "nt" else "link-peer")
        run(flags + ["firmware/src/frame.c", "firmware/src/identity.c", "firmware/src/live_data.c",
                     "firmware/src/calibration.c", "firmware/src/map_read.c", "firmware/src/dispatch.c",
                     "firmware/src/stream_rx.c", "firmware/src/stream_tx.c", "firmware/src/link.c",
                     "firmware/src/crank.c", "firmware/src/position.c", "firmware/src/capture_queue.c",
                     "firmware/src/capture_processor.c", "firmware/src/telemetry.c",
                     "firmware/src/analog.c", "firmware/src/clock32.c", "firmware/src/capture_source.c",
                     "firmware/src/live_mailbox.c",
                     "unit_tests/link_peer.c", "-lm", "-o", str(peer_exe)])
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
        java_root = ROOT / "java_console/src/main/java/io/tunuino"
        sources = [java_root / "protocol/FrameCodec.java", java_root / "datalog/LogSample.java",
                   java_root / "datalog/CsvLogReader.java", java_root / "transport/SerialChannel.java",
                   java_root / "transport/PacketSession.java",
                   java_root / "protocol/DeviceIdentity.java", java_root / "protocol/IdentityClient.java",
                   java_root / "protocol/MapReadClient.java",
                   java_root / "protocol/LiveDataClient.java", java_root / "protocol/IoClient.java",
                    java_root / "protocol/EnginePreviewClient.java",
                    java_root / "protocol/EngineTimingClient.java", java_root / "protocol/MapWriteClient.java", java_root / "datalog/CsvLogWriter.java",
                   java_root / "datalog/LiveDataRecorder.java",
                   java_root / "datalog/LiveDataMonitor.java", java_root / "datalog/LogChannel.java",
                   java_root / "datalog/DatalogAnalysis.java",
                   java_root / "calibration/CalibrationMap.java",
                   java_root / "calibration/CalibrationDocument.java",
                   ROOT / "java_console/src/test/java/io/tunuino/CoreChecks.java",
                   ROOT / "java_console/src/test/java/io/tunuino/TransportChecks.java",
                   ROOT / "java_console/src/test/java/io/tunuino/IdentityChecks.java",
                   ROOT / "java_console/src/test/java/io/tunuino/LiveDataChecks.java",
                   ROOT / "java_console/src/test/java/io/tunuino/CalibrationChecks.java",
                   ROOT / "java_console/src/test/java/io/tunuino/MapReadChecks.java",
                   ROOT / "java_console/src/test/java/io/tunuino/FirmwareLinkChecks.java",
                   ROOT / "java_console/src/test/java/io/tunuino/IoChecks.java",
                   ROOT / "java_console/src/test/java/io/tunuino/TelemetryChecks.java"]
        sources.extend([java_root / "calibration/IoConfiguration.java", java_root / "protocol/ConfigurationClient.java",
            ROOT / "java_console/src/test/java/io/tunuino/ConfigurationChecks.java"])
        sources.append(java_root / "transport/ProcessSerialChannel.java")
        sources.append(java_root / "calibration/EngineProfile.java")
        sources.append(java_root / "calibration/FuelCorrections.java")
        sources.append(java_root / "calibration/AuxConfiguration.java")
        sources.append(java_root / "protocol/AuxControlClient.java")
        sources.append(java_root / "calibration/FeatureConfiguration.java")
        sources.append(java_root / "calibration/IoAssignments.java")
        sources.append(java_root / "calibration/ProteusRouting.java")
        sources.append(java_root / "protocol/IoAssignmentClient.java")
        sources.append(ROOT / "java_console/src/test/java/io/tunuino/AuxControlChecks.java")
        sources.append(java_root / "calibration/BaseMaps.java")
        sources.append(ROOT / "java_console/src/test/java/io/tunuino/BaseConfigurationChecks.java")
        sources.extend([java_root/"calibration/EnginePreset.java",ROOT/"java_console/src/test/java/io/tunuino/EnginePresetChecks.java"])
        sources.extend([java_root/"protocol/EngineOutputClient.java",ROOT/"java_console/src/test/java/io/tunuino/EngineOutputChecks.java"])
        sources.extend(sorted((ROOT / "java_tools/src/main/java").rglob("*.java")))
        sources.extend(sorted((ROOT / "java_tools/src/test/java").rglob("*.java")))
        javac = ["javac"] if shutil.which("javac") else ["java", "--module", "jdk.compiler/com.sun.tools.javac.Main"]
        run(javac + ["-encoding", "UTF-8", "--release", "17", "-d", str(work)] + [str(p) for p in sources])
        print(run(["java", "-cp", str(work), "io.tunuino.CoreChecks", c_vector]))
        print(run(["java", "-cp", str(work), "io.tunuino.TransportChecks"]))
        print(run(["java", "-cp", str(work), "io.tunuino.IdentityChecks", identity_vector]))
        print(run(["java", "-cp", str(work), "io.tunuino.LiveDataChecks", live_vector]))
        print(run(["java", "-cp", str(work), "io.tunuino.CalibrationChecks", map_vector, ignition_vector, str(refined_file)]))
        read_fixture = work / "map-read-vectors.txt"
        read_fixture.write_text("\n".join(read_output[:4]), encoding="ascii")
        print(run(["java", "-cp", str(work), "io.tunuino.MapReadChecks", str(read_fixture)]))
        print(run(["java", "-cp", str(work), "io.tunuino.FirmwareLinkChecks", str(peer_exe)], timeout=30))
        print(run(["java", "-cp", str(work), "io.tunuino.IoChecks", str(io_peer)], timeout=30))
        print(run(["java", "-cp", str(work), "io.tunuino.ConfigurationChecks", str(io_peer)], timeout=30))
        print(run(["java","-cp",str(work),"io.tunuino.EngineOutputChecks",str(output_peer)],timeout=30))
        print(run(["java","-cp",str(work),"io.tunuino.EnginePresetChecks"]))
        print(run(["java","-cp",str(work),"io.tunuino.AuxControlChecks",str(output_peer)],timeout=45))
        print(run(["java","-cp",str(work),"io.tunuino.BaseConfigurationChecks"]))
        simulator = build_simulator(work / "simulator")
        print(run(["java", "-cp", str(work), "io.tunuino.TelemetryChecks", str(simulator)], timeout=30))
        print(run(["java", "-cp", str(work), "io.tunuino.tools.ToolChecks", str(simulator)], timeout=45))
    print("Portable core verified. JavaFX UI and hardware builds require separate validation.")


if __name__ == "__main__":
    main()
