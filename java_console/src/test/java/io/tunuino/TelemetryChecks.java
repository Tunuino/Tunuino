/* Copyright (c) 2026 Tunuino. */
package io.tunuino;

import io.tunuino.datalog.CsvLogReader;
import io.tunuino.datalog.DatalogAnalysis;
import io.tunuino.datalog.LiveDataMonitor;
import io.tunuino.datalog.LogChannel;
import io.tunuino.datalog.LogSample;
import io.tunuino.protocol.DeviceIdentity;
import io.tunuino.protocol.FrameCodec;
import io.tunuino.protocol.IdentityClient;
import io.tunuino.transport.PacketSession;
import io.tunuino.transport.ProcessSerialChannel;
import io.tunuino.transport.SerialChannel;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

public final class TelemetryChecks {
    private static void check(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }

    private static void analysis() {
        List<LogSample> samples = new ArrayList<>();
        for (int i = 0; i < 10_001; ++i) samples.add(new LogSample(i, 1000, 100, 1, 90));
        samples.set(41, new LogSample(41, 8000, 90, 0.9, -10));
        samples.set(43, new LogSample(43, 0, 110, 1.1, 110));
        var reduced = DatalogAnalysis.chartSamples(samples, LogChannel.RPM, 8);
        check(reduced.size() <= 8 && reduced.get(0) == samples.get(0)
            && reduced.get(reduced.size() - 1) == samples.get(10_000), "Endpoints and size bound");
        check(reduced.contains(samples.get(41)) && reduced.contains(samples.get(43)), "Short peak and trough preserved");
        for (int i = 1; i < reduced.size(); ++i) {
            check(reduced.get(i).timeMs() >= reduced.get(i - 1).timeMs(), "Reduced data stays ordered");
        }
        check(DatalogAnalysis.range(samples, LogChannel.RPM).equals(new DatalogAnalysis.Range(0, 8000)), "RPM range");
        check(DatalogAnalysis.range(samples, LogChannel.COOLANT).equals(new DatalogAnalysis.Range(-10, 110)), "Signed temperatures");
        for (LogChannel channel : LogChannel.values()) {
            check(Double.isFinite(channel.value(samples.get(0))), "Channel mapping");
        }
        List<LogSample> duplicates = List.of(samples.get(0), samples.get(0), samples.get(1));
        check(DatalogAnalysis.chartSamples(duplicates, LogChannel.LAMBDA, 4).size() == 3, "Equal timestamps retained");
        try { reduced.clear(); throw new AssertionError("Mutable analysis result"); }
        catch (UnsupportedOperationException expected) { /* immutable */ }
    }

    private static void recording(Path simulator, Path directory) throws Exception {
        try (PacketSession session = new PacketSession(ProcessSerialChannel.simulator(simulator, "proteus", "normal"))) {
            DeviceIdentity identity = IdentityClient.identify(session, DeviceIdentity.Family.PROTEUS, Duration.ofSeconds(2));
            LiveDataMonitor monitor = new LiveDataMonitor(session, identity, Duration.ofMillis(10));
            Path existing = directory.resolve("existing.csv"); Files.writeString(existing, "keep");
            var refused = monitor.startRecording(existing);
            Path first = directory.resolve("simulated-first.csv"), second = directory.resolve("simulated-second.csv");
            AtomicReference<CompletableFuture<LiveDataMonitor.Recording>> begun = new AtomicReference<>();
            AtomicReference<CompletableFuture<LiveDataMonitor.Recording>> duplicate = new AtomicReference<>();
            AtomicReference<CompletableFuture<LiveDataMonitor.Recording>> stopped = new AtomicReference<>();
            AtomicReference<CompletableFuture<LiveDataMonitor.Recording>> pending = new AtomicReference<>();
            AtomicInteger ticks = new AtomicInteger(); AtomicBoolean running = new AtomicBoolean(true);
            long count = monitor.run(running::get, sample -> {
                check(sample.isPresent(), "Normal simulator sample");
                switch (ticks.incrementAndGet()) {
                    case 1 -> {
                        check(refused.isCompletedExceptionally() && monitor.recording() == null, "Existing file rejected without stopping monitoring");
                        begun.set(monitor.startRecording(first));
                    }
                    case 2 -> {
                        check(begun.get().isDone() && monitor.recording().samples() == 1, "Recording starts during monitoring");
                        duplicate.set(monitor.startRecording(second));
                    }
                    case 3 -> {
                        check(duplicate.get().isCompletedExceptionally() && !Files.exists(second), "Duplicate request preserves active log");
                        stopped.set(monitor.stopRecording());
                    }
                    case 4 -> {
                        check(stopped.get().join().samples() == 2 && monitor.recording() == null, "Stop CSV keeps polling");
                        begun.set(monitor.startRecording(second));
                    }
                    case 6 -> {
                        check(monitor.recording().samples() == 2, "Second recording has independent counter");
                        pending.set(monitor.startRecording(directory.resolve("never-created.csv")));
                        running.set(false);
                    }
                    default -> { }
                }
            }, error -> { throw new AssertionError(error); });
            check(count == 6 && session.isOpen(), "Stop monitor preserves serial session");
            check(monitor.recording() == null && pending.get().isCompletedExceptionally(), "Final cleanup resolves queued commands");
            check(monitor.startRecording(first).isCompletedExceptionally(), "Finished monitor rejects work");
            check(Files.readString(existing).equals("keep") && !Files.exists(directory.resolve("never-created.csv")), "No overwrite or late writes");
            check(CsvLogReader.read(first).size() == 2 && CsvLogReader.read(second).size() == 2, "CSV finalization and reopening");
        }
    }

    private static void recovery(Path simulator) throws Exception {
        for (String scenario : List.of("sensor-loss", "stall", "wrap")) {
            try (PacketSession session = new PacketSession(ProcessSerialChannel.simulator(simulator, "proteus", scenario))) {
                var identity = IdentityClient.identify(session, DeviceIdentity.Family.PROTEUS, Duration.ofSeconds(2));
                LiveDataMonitor monitor = new LiveDataMonitor(session, identity, Duration.ofMillis(10));
                AtomicInteger ticks = new AtomicInteger(), missing = new AtomicInteger();
                AtomicBoolean running = new AtomicBoolean(true);
                AtomicReference<Optional<LogSample>> last = new AtomicReference<>();
                long count = monitor.run(running::get, sample -> {
                    if (sample.isEmpty()) missing.incrementAndGet();
                    last.set(sample);
                    if (ticks.incrementAndGet() == 24) running.set(false);
                }, error -> { throw new AssertionError(error); });
                check(count + missing.get() == 24 && count > 10 && last.get().isPresent(), "Availability and recovery: " + scenario);
                check((missing.get() > 0) == !scenario.equals("wrap"), "Missing data is explicit: " + scenario);
                check(session.isOpen(), "Recovery preserves connection");
            }
        }
    }

    private static byte[] sample(long time) {
        return FrameCodec.encode(ByteBuffer.allocate(28).put(new byte[] {0, 1, 15, 0}).putLong(time)
            .putInt(1000000).putInt(100000).putInt(1000000).putInt(90000).array());
    }

    private static void failures(Path directory) throws Exception {
        for (boolean backwards : List.of(false, true)) {
            byte[] first = sample(1000), last = backwards ? sample(999) : FrameCodec.encode(new byte[] {0, 2});
            byte[] responses = new byte[first.length + last.length];
            System.arraycopy(first, 0, responses, 0, first.length);
            System.arraycopy(last, 0, responses, first.length, last.length);
            ByteArrayInputStream input = new ByteArrayInputStream(responses);
            SerialChannel channel = new SerialChannel() {
                @Override public int read(byte[] b, int o, int n, int timeout) { return input.read(b, o, n); }
                @Override public int write(byte[] b, int o, int n, int timeout) { return n; }
                @Override public void close() { }
            };
            try (PacketSession session = new PacketSession(channel)) {
                var identity = new DeviceIdentity(DeviceIdentity.Family.PROTEUS, DeviceIdentity.McuSeries.STM32_F4,
                    7, 0, 0, 3, 1, "0102030405060708090a0b0c");
                LiveDataMonitor monitor = new LiveDataMonitor(session, identity, Duration.ofMillis(10));
                Path csv = directory.resolve("partial-" + backwards + ".csv");
                monitor.startRecording(csv);
                try {
                    monitor.run(() -> true, sample -> {}, error -> { throw new AssertionError(error); });
                    throw new AssertionError("Bad sample accepted");
                } catch (IOException expected) {
                    check(!session.isOpen() && monitor.recording() == null, "Failure clears session and recording");
                    check(CsvLogReader.read(csv).size() == 1, "Partial log preserved on failure");
                }
            }
        }
    }

    public static void main(String[] args) throws Exception {
        analysis();
        Path directory = Files.createTempDirectory("tunuino-telemetry-");
        try {
            recording(Path.of(args[0]), directory); recovery(Path.of(args[0])); failures(directory);
        } finally {
            try (var files = Files.list(directory)) { for (Path path : files.toList()) Files.delete(path); }
            Files.delete(directory);
        }
        System.out.println("Telemetry: actual C simulator, independent CSV start/stop, unavailable data, recovery, clock rollback, partial logs and peak-preserving analysis passed");
    }
}
