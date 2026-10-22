/* Copyright (c) 2026 Tunuino. */
package io.tunuino;

import io.tunuino.calibration.CalibrationMap;
import io.tunuino.datalog.CsvLogReader;
import io.tunuino.datalog.LiveDataRecorder;
import io.tunuino.protocol.DeviceIdentity;
import io.tunuino.protocol.IdentityClient;
import io.tunuino.protocol.LiveDataClient;
import io.tunuino.protocol.MapReadClient;
import io.tunuino.transport.PacketSession;
import io.tunuino.transport.SerialChannel;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Duration;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

/** Test-only pipe adapter; it neither replaces USB nor validates native drivers. */
public final class FirmwareLinkChecks {
    private static void check(boolean value, String message) { if (!value) { throw new AssertionError(message); } }
    private static final class Peer implements SerialChannel {
        final Process process;
        final InputStream input;
        final OutputStream output;
        boolean closed;
        int writes, reads;
        Peer(String executable, String family, String mode) throws IOException {
            process = new ProcessBuilder(executable,family,mode).redirectError(ProcessBuilder.Redirect.INHERIT).start();
            input = process.getInputStream(); output = process.getOutputStream();
        }
        public int read(byte[] bytes, int offset, int count, int timeoutMs) throws IOException {
            long deadline = System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(timeoutMs);
            while (!closed) {
                int ready = input.available();
                if (ready > 0) { ++reads; return input.read(bytes,offset,Math.min(Math.min(count,7),ready)); }
                if (!process.isAlive() && input.available() == 0) { return -1; }
                if (System.nanoTime() - deadline >= 0) { return 0; }
                try { Thread.sleep(1); } catch (InterruptedException e) {
                    Thread.currentThread().interrupt(); throw new java.io.InterruptedIOException("Pipe read interrupted");
                }
            }
            return -1;
        }
        public int write(byte[] bytes, int offset, int count, int timeoutMs) throws IOException {
            if (closed) { return -1; }
            int written = Math.min(count,3); output.write(bytes,offset,written); output.flush(); ++writes; return written;
        }
        public void close() throws IOException {
            if (closed) { return; }
            closed = true;
            try {
                try { output.close(); } finally { input.close(); }
            } finally {
                try {
                    if (!process.waitFor(1,TimeUnit.SECONDS)) {
                        process.destroyForcibly();
                        if (!process.waitFor(1,TimeUnit.SECONDS)) { throw new IOException("C peer did not terminate"); }
                    }
                } catch (InterruptedException e) {
                    process.destroyForcibly(); Thread.currentThread().interrupt();
                    throw new java.io.InterruptedIOException("Peer cleanup interrupted");
                }
            }
        }
    }
    private static DeviceIdentity identify(PacketSession session, DeviceIdentity.Family family) throws IOException {
        return IdentityClient.identify(session,family,Duration.ofSeconds(2));
    }
    private static void failedIdentity(String executable, String mode, DeviceIdentity.Family expected) throws Exception {
        Peer peer = new Peer(executable,"proteus",mode);
        try (PacketSession session = new PacketSession(peer)) {
            try { identify(session,expected); throw new AssertionError("Bad identity exchange accepted"); }
            catch (IOException e) { check(!session.isOpen() && peer.closed,"Identity failure closes actual peer"); }
        }
    }
    public static void main(String[] args) throws Exception {
        String executable = args[0];
        Path directory = Files.createTempDirectory("tunuino-link-java-");
        try {
            for (DeviceIdentity.Family family : DeviceIdentity.Family.values()) {
                Peer peer = new Peer(executable,family == DeviceIdentity.Family.PROTEUS ? "proteus" : "uaefi","normal");
                try (PacketSession session = new PacketSession(peer)) {
                    DeviceIdentity identity = identify(session,family);
                    check(identity.family() == family && identity.capabilities() == 3,"C identity family and capabilities");
                    check(identity.uid().equals("0102030405060708090a0b0c"),"Synthetic UID unchanged across actual processes");
                    var sample = LiveDataClient.read(session,identity,Duration.ofSeconds(2)).orElseThrow();
                    check(sample.timeMs() == 1235 && sample.rpm() == 950.125 && sample.mapKpa() == 98.765
                        && sample.lambda() == 1.023456 && sample.coolantC() == -12.345,"Actual C live-data scaling");
                    for (CalibrationMap.Kind kind : CalibrationMap.Kind.values()) {
                        CalibrationMap map = MapReadClient.read(session,identity,kind,Duration.ofSeconds(5));
                        check(map.rows() == 32 && map.columns() == 32 && map.encode().length == 4368,"Maximum C map transferred");
                        for (int row = 0; row < 32; ++row) {
                            for (int col = 0; col < 32; ++col) {
                                double expected = (row * 1000 + col + (kind == CalibrationMap.Kind.VE ? 40000 : -15000)) / 1000.0;
                                check(map.rpmAt(col) == col * 250 && map.loadKpaAt(row) == row * 10
                                    && map.cell(row,col) == expected,"Every C map axis and cell preserved");
                            }
                        }
                        Path file = directory.resolve(family.name() + "-" + kind.name() + ".tnmap");
                        map.writeNew(file);
                        check(java.util.Arrays.equals(map.encode(),CalibrationMap.read(file).encode()),"Transferred map opens offline");
                    }
                    AtomicBoolean recording = new AtomicBoolean(true); AtomicInteger count = new AtomicInteger();
                    Path csv = directory.resolve(family.name() + ".csv");
                    long saved = LiveDataRecorder.record(session,identity,csv,Duration.ofMillis(10),recording::get,
                        sampleValue -> { if (count.incrementAndGet() == 3) { recording.set(false); } });
                    var samples = CsvLogReader.read(csv);
                    check(saved == 3 && samples.size() == 3 && samples.get(2).timeMs() > samples.get(0).timeMs(),"C live stream records and reopens CSV");
                    check(session.isOpen() && peer.reads > 100 && peer.writes > 100,"Fragmented bidirectional exchanges preserve session");
                }
                check(peer.process.exitValue() == 0,"Normal C peer terminated cleanly");
            }
            for (DeviceIdentity.Family family : DeviceIdentity.Family.values()) {
                String board = family == DeviceIdentity.Family.PROTEUS ? "proteus" : "uaefi";
                for (String captureMode : new String[] {"crank-live", "capture-wrap"}) {
                    Peer measured = new Peer(executable,board,captureMode);
                    try (PacketSession session = new PacketSession(measured)) {
                        DeviceIdentity identity = identify(session,family);
                        var sample = LiveDataClient.read(session,identity,Duration.ofSeconds(2)).orElseThrow();
                        check(Math.abs(sample.rpm() - 1666.667) < 1e-9 && sample.mapKpa() == 98.765
                            && sample.lambda() == 1.023456 && sample.coolantC() == -12.345,"Decoded crank RPM reaches Java with sensor scalings");
                        AtomicBoolean recording = new AtomicBoolean(true); AtomicInteger count = new AtomicInteger();
                        Path csv = directory.resolve(family.name() + "-" + captureMode + "-decoded.csv");
                        long saved = LiveDataRecorder.record(session,identity,csv,Duration.ofMillis(10),recording::get,
                            value -> { if (count.incrementAndGet() == 45) { recording.set(false); } });
                        var samples = CsvLogReader.read(csv);
                        check(saved == 45 && samples.size() == 45 && session.isOpen(),"Decoded crank CSV completes normally");
                        boolean crossedGap = false;
                        for (int i = 0; i < samples.size(); ++i) {
                            check(Math.abs(samples.get(i).rpm() - 1666.667) < 1e-9,"Missing-tooth interval does not lower logged RPM");
                            if (i > 0) {
                                long delta = samples.get(i).timeMs() - samples.get(i - 1).timeMs();
                                check(delta == 1 || delta == 2,"Capture timestamps survive the telemetry path");
                                crossedGap |= delta == 2;
                            }
                        }
                        check(crossedGap,"Recording spans a missing-tooth gap");
                        if (captureMode.equals("capture-wrap")) {
                            long wrapMs = (1L << 32) / 1000;
                            check(samples.get(0).timeMs() < wrapMs
                                && samples.get(samples.size() - 1).timeMs() > wrapMs,
                                "CSV timestamps remain continuous across the physical 32-bit counter wrap");
                        }
                    }
                    check(measured.process.exitValue() == 0,"Decoded-crank peer terminates cleanly");
                }
                for (String captureMode : new String[] {"crank-stall", "capture-loss"}) {
                    Peer stopped = new Peer(executable,board,captureMode);
                    try (PacketSession session = new PacketSession(stopped)) {
                        DeviceIdentity identity = identify(session,family);
                        check(LiveDataClient.read(session,identity,Duration.ofSeconds(2)).isPresent(),"Crank initially available");
                        check(LiveDataClient.read(session,identity,Duration.ofSeconds(2)).isEmpty(),"Capture timeout/loss produces NOT_READY");
                        AtomicInteger budget = new AtomicInteger();
                        Path csv = directory.resolve(family.name() + "-" + captureMode + "-stopped.csv");
                        long saved = LiveDataRecorder.record(session,identity,csv,Duration.ofMillis(10),
                            () -> budget.getAndIncrement() < 6,value -> { throw new AssertionError("Stale crank sample delivered"); });
                        check(saved == 0 && Files.readAllLines(csv).equals(java.util.List.of("time_ms,rpm,map_kpa,lambda,coolant_c"))
                            && session.isOpen(),"Stopped crank does not create CSV rows or close a healthy link");
                    }
                    check(stopped.process.exitValue() == 0,"Stopped-crank peer terminates cleanly");
                }
            }
            for (DeviceIdentity.Family family : DeviceIdentity.Family.values()) {
                String board = family == DeviceIdentity.Family.PROTEUS ? "proteus" : "uaefi";
                Peer expiring = new Peer(executable,board,"mailbox-expiry");
                try (PacketSession session = new PacketSession(expiring)) {
                    DeviceIdentity identity = identify(session,family);
                    long first = -1;
                    for (int step = 0; step < 4; ++step) {
                        var result = LiveDataClient.read(session,identity,Duration.ofSeconds(2));
                        check(result.isPresent() == (step == 0 || step == 3),
                            "Mailbox publication stops, expires and resumes at request boundaries");
                        if (result.isPresent()) {
                            var sample = result.orElseThrow();
                            check(sample.rpm() == 1666.667,"Fresh mailbox preserves measured RPM");
                            if (step == 0) { first = sample.timeMs(); }
                            else { check(sample.timeMs() - first == 21,"Recovery has a new acquisition timestamp"); }
                        }
                        check(session.isOpen(),"Mailbox expiry keeps communication open");
                    }
                }
                check(expiring.process.exitValue() == 0,"Expiring mailbox peer terminates cleanly");
                Peer recordingPeer = new Peer(executable,board,"mailbox-expiry");
                try (PacketSession session = new PacketSession(recordingPeer)) {
                    DeviceIdentity identity = identify(session,family);
                    AtomicInteger budget = new AtomicInteger(), delivered = new AtomicInteger();
                    Path csv = directory.resolve(family.name() + "-mailbox-expiry.csv");
                    long saved = LiveDataRecorder.record(session,identity,csv,Duration.ofMillis(10),
                        () -> budget.getAndIncrement() < 8,value -> delivered.incrementAndGet());
                    var samples = CsvLogReader.read(csv);
                    check(saved == 2 && delivered.get() == 2 && samples.size() == 2
                        && samples.get(1).timeMs() - samples.get(0).timeMs() == 21,
                        "CSV skips expired mailbox data and resumes with the real time gap");
                    check(session.isOpen(),"Mailbox recording resumes without reconnecting");
                }
                check(recordingPeer.process.exitValue() == 0,"Mailbox expiry recording peer terminates cleanly");
            }
            for (DeviceIdentity.Family family : DeviceIdentity.Family.values()) {
                String board = family == DeviceIdentity.Family.PROTEUS ? "proteus" : "uaefi";
                Peer peer = new Peer(executable,board,"capture-loss");
                try (PacketSession session = new PacketSession(peer)) {
                    DeviceIdentity identity = identify(session,family);
                    AtomicInteger budget = new AtomicInteger(), delivered = new AtomicInteger();
                    Path csv = directory.resolve(family.name() + "-capture-loss-during-recording.csv");
                    long saved = LiveDataRecorder.record(session,identity,csv,Duration.ofMillis(10),
                        () -> budget.getAndIncrement() < 12,value -> delivered.incrementAndGet());
                    var samples = CsvLogReader.read(csv);
                    check(saved == 1 && delivered.get() == 1 && samples.size() == 1
                        && samples.get(0).rpm() == 1666.667,
                        "Capture loss retains the preceding CSV row and suppresses later rows");
                    check(session.isOpen(),"Acquisition failure preserves a healthy protocol connection");
                }
                check(peer.process.exitValue() == 0,"Capture-loss recording peer terminates cleanly");
            }
            for (DeviceIdentity.Family family : DeviceIdentity.Family.values()) {
                String board = family == DeviceIdentity.Family.PROTEUS ? "proteus" : "uaefi";
                Peer sensors = new Peer(executable,board,"sensor-recover");
                try (PacketSession session = new PacketSession(sensors)) {
                    DeviceIdentity identity = identify(session,family);
                    long previous = -1;
                    for (int step = 0; step < 8; ++step) {
                        var result = LiveDataClient.read(session,identity,Duration.ofSeconds(2));
                        boolean available = step == 0 || step == 4 || step == 7;
                        check(result.isPresent() == available,"Raw sensor fault/freshness sequence step " + step);
                        if (available) {
                            var sample = result.orElseThrow();
                            check(sample.rpm() == 1666.667 && sample.mapKpa() == 98.765
                                && sample.lambda() == 1.023456 && sample.coolantC() == -12.345,
                                "Interpolated raw readings and decoded RPM reach Java");
                            check(sample.timeMs() > previous,"Recovery supplies a new timestamp");
                            previous = sample.timeMs();
                        }
                        check(session.isOpen(),"Sensor unavailability preserves the communication session");
                    }
                }
                check(sensors.process.exitValue() == 0,"Sensor sequence peer terminates cleanly");
                Peer recordingPeer = new Peer(executable,board,"sensor-recover");
                try (PacketSession session = new PacketSession(recordingPeer)) {
                    DeviceIdentity identity = identify(session,family);
                    AtomicInteger budget = new AtomicInteger(), delivered = new AtomicInteger();
                    Path csv = directory.resolve(family.name() + "-sensor-recovery.csv");
                    // Two running checks per iteration bound this recording to eight requests.
                    long saved = LiveDataRecorder.record(session,identity,csv,Duration.ofMillis(10),
                        () -> budget.getAndIncrement() < 16,value -> delivered.incrementAndGet());
                    var samples = CsvLogReader.read(csv);
                    check(saved == 3 && delivered.get() == 3 && samples.size() == 3,
                        "Recorder skips all five invalid sensor states and resumes twice");
                    check(samples.get(1).timeMs() - samples.get(0).timeMs() == 4
                        && samples.get(2).timeMs() - samples.get(1).timeMs() == 3,
                        "CSV retains acquisition-time gaps across unavailable sensors");
                    for (var sample : samples) {
                        check(sample.rpm() == 1666.667 && sample.mapKpa() == 98.765
                            && sample.lambda() == 1.023456 && sample.coolantC() == -12.345,
                            "CSV contains calibrated measurements rather than zero substitutes");
                    }
                    check(session.isOpen(),"Recording recovers without reconnecting");
                }
                check(recordingPeer.process.exitValue() == 0,"Sensor recovery recorder peer terminates cleanly");
            }
            for (String mode : new String[] {"stale-map","drop-mid-map","not-ready"}) {
                Peer peer = new Peer(executable,"proteus",mode);
                try (PacketSession session = new PacketSession(peer)) {
                    DeviceIdentity identity = identify(session,DeviceIdentity.Family.PROTEUS);
                    if (mode.equals("not-ready")) {
                        check(LiveDataClient.read(session,identity,Duration.ofSeconds(2)).isEmpty(),"C unavailable sample is absent");
                    }
                    try {
                        MapReadClient.read(session,identity,CalibrationMap.Kind.VE,Duration.ofSeconds(3));
                        throw new AssertionError("Partial/stale/unavailable map was delivered");
                    } catch (IOException expected) { check(!session.isOpen() && peer.closed,"Map failure closes actual peer"); }
                }
                check(peer.process.exitValue() == 0,"Fault-injection peer terminated cleanly");
            }
            failedIdentity(executable,"normal",DeviceIdentity.Family.UAEFI);
            failedIdentity(executable,"bad-crc",DeviceIdentity.Family.PROTEUS);
            failedIdentity(executable,"truncated",DeviceIdentity.Family.PROTEUS);
        } finally {
            try (var paths = Files.list(directory)) { for (Path file : paths.toList()) { Files.delete(file); } }
            Files.delete(directory);
        }
        System.out.println("Java clients <-> live C link: both families, full maps, live CSV, fragmentation, stale snapshot, CRC, disconnect, mailbox expiry/recovery, capture-source wrap/loss, decoded-crank and calibrated-sensor recovery checks passed");
    }
}
