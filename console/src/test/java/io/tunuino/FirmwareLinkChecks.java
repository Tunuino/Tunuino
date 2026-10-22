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
                Peer measured = new Peer(executable,board,"crank-live");
                try (PacketSession session = new PacketSession(measured)) {
                    DeviceIdentity identity = identify(session,family);
                    var sample = LiveDataClient.read(session,identity,Duration.ofSeconds(2)).orElseThrow();
                    check(Math.abs(sample.rpm() - 1666.667) < 1e-9 && sample.mapKpa() == 98.765
                        && sample.lambda() == 1.023456 && sample.coolantC() == -12.345,"Decoded crank RPM reaches Java with sensor scalings");
                    AtomicBoolean recording = new AtomicBoolean(true); AtomicInteger count = new AtomicInteger();
                    Path csv = directory.resolve(family.name() + "-decoded.csv");
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
                }
                check(measured.process.exitValue() == 0,"Decoded-crank peer terminates cleanly");
                Peer stopped = new Peer(executable,board,"crank-stall");
                try (PacketSession session = new PacketSession(stopped)) {
                    DeviceIdentity identity = identify(session,family);
                    check(LiveDataClient.read(session,identity,Duration.ofSeconds(2)).isPresent(),"Crank initially available");
                    check(LiveDataClient.read(session,identity,Duration.ofSeconds(2)).isEmpty(),"Crank timeout produces NOT_READY");
                    AtomicInteger budget = new AtomicInteger();
                    Path csv = directory.resolve(family.name() + "-stopped.csv");
                    long saved = LiveDataRecorder.record(session,identity,csv,Duration.ofMillis(10),
                        () -> budget.getAndIncrement() < 6,value -> { throw new AssertionError("Stale crank sample delivered"); });
                    check(saved == 0 && Files.readAllLines(csv).equals(java.util.List.of("time_ms,rpm,map_kpa,lambda,coolant_c"))
                        && session.isOpen(),"Stopped crank does not create CSV rows or close a healthy link");
                }
                check(stopped.process.exitValue() == 0,"Stopped-crank peer terminates cleanly");
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
        System.out.println("Java clients <-> live C link: both families, full maps, live CSV, fragmentation, stale snapshot, CRC, disconnect and decoded-crank telemetry checks passed");
    }
}
