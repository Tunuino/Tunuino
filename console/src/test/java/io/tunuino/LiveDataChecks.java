/* Copyright (c) 2026 Tunuino. */
package io.tunuino;

import io.tunuino.datalog.CsvLogReader;
import io.tunuino.datalog.CsvLogWriter;
import io.tunuino.datalog.LiveDataRecorder;
import io.tunuino.datalog.LogSample;
import io.tunuino.protocol.DeviceIdentity;
import io.tunuino.protocol.FrameCodec;
import io.tunuino.protocol.LiveDataClient;
import io.tunuino.transport.PacketSession;
import io.tunuino.transport.SerialChannel;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.file.FileAlreadyExistsException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Duration;
import java.util.Arrays;
import java.util.HexFormat;
import java.util.Locale;
import java.util.concurrent.atomic.AtomicBoolean;

public final class LiveDataChecks {
    private static void check(boolean condition, String message) {
        if (!condition) { throw new AssertionError(message); }
    }

    private static DeviceIdentity identity(long capabilities) {
        return new DeviceIdentity(DeviceIdentity.Family.PROTEUS, DeviceIdentity.McuSeries.STM32_F4,
            1, 0, 1, 0, capabilities, "0102030405060708090a0b0c");
    }

    private static final class Channel implements SerialChannel {
        final ByteArrayInputStream response;
        final ByteArrayOutputStream written = new ByteArrayOutputStream();
        int closes;
        Channel(byte[] response) { this.response = new ByteArrayInputStream(response); }
        @Override public int read(byte[] b, int o, int n, int ms) { return response.read(b, o, Math.min(n, 3)); }
        @Override public int write(byte[] b, int o, int n, int ms) { written.write(b, o, n); return n; }
        @Override public void close() { ++closes; }
    }

    private static void badResponse(byte[] body) throws Exception {
        Channel channel = new Channel(FrameCodec.encode(body));
        try (PacketSession session = new PacketSession(channel)) {
            try {
                LiveDataClient.read(session, identity(1), Duration.ofSeconds(1));
                throw new AssertionError("Invalid sample accepted");
            } catch (IOException expected) {
                check(!session.isOpen() && channel.closes == 1, "Invalid sample closes session");
            }
        }
    }

    public static void main(String[] args) throws Exception {
        byte[] cPacket = HexFormat.of().parseHex(args[0]);
        byte[] body = FrameCodec.decode(cPacket);
        LogSample sample = LiveDataClient.decode(body).orElseThrow();
        check(sample.timeMs() == 0x123456789ABL, "64-bit timestamp");
        check(Math.abs(sample.rpm() - 950.125) < 1e-9, "RPM scaling");
        check(Math.abs(sample.mapKpa() - 98.765) < 1e-9, "MAP scaling");
        check(Math.abs(sample.lambda() - 1.023456) < 1e-9, "Lambda scaling");
        check(Math.abs(sample.coolantC() + 12.345) < 1e-9, "Signed coolant temperature");
        check(LiveDataClient.decode(new byte[] {1}).isEmpty(), "Unavailable sample is absent");
        for (int offset : new int[] {0, 1, 2, 3, 4}) {
            byte[] bad = body.clone();
            bad[offset] = (byte) 0xFF;
            badResponse(bad);
        }
        badResponse(Arrays.copyOf(body, 27));
        badResponse(Arrays.copyOf(body, 29));
        Channel channel = new Channel(cPacket);
        try (PacketSession session = new PacketSession(channel)) {
            try {
                LiveDataClient.read(session, identity(0), Duration.ofSeconds(1));
                throw new AssertionError("Capability was ignored");
            } catch (IllegalStateException expected) {
                check(session.isOpen() && channel.written.size() == 0, "No request without capability");
            }
        }

        Path directory = Files.createTempDirectory("tunuino-live-");
        Locale previous = Locale.getDefault();
        try {
            Locale.setDefault(Locale.GERMANY);
            Path csv = directory.resolve("record.csv");
            byte[] notReady = FrameCodec.encode(new byte[] {1});
            byte[] replies = new byte[notReady.length + cPacket.length];
            System.arraycopy(notReady, 0, replies, 0, notReady.length);
            System.arraycopy(cPacket, 0, replies, notReady.length, cPacket.length);
            Channel recording = new Channel(replies);
            AtomicBoolean running = new AtomicBoolean(true);
            try (PacketSession session = new PacketSession(recording)) {
                long count = LiveDataRecorder.record(session, identity(1), csv, Duration.ofMillis(10),
                    running::get, value -> { check(value.equals(sample), "Delivered sample"); running.set(false); });
                check(count == 1 && session.isOpen(), "Normal stop preserves connection");
                byte[] query = FrameCodec.encode(LiveDataClient.request());
                check(recording.written.size() == query.length * 2, "Two read requests, one valid sample");
                check(Arrays.equals(query, Arrays.copyOf(recording.written.toByteArray(), query.length)), "Exact live-data query");
            }
            check(CsvLogReader.read(csv).equals(java.util.List.of(sample)), "Live/offline CSV interoperability");
            check(Files.readString(csv).contains("950.125,98.765,1.023456,-12.345"), "Locale-independent decimals");
            String original = Files.readString(csv);
            try {
                new CsvLogWriter(csv);
                throw new AssertionError("Existing file overwritten");
            } catch (FileAlreadyExistsException expected) {
                check(Files.readString(csv).equals(original), "Existing log preserved");
            }
            Path backwards = directory.resolve("backwards.csv");
            CsvLogWriter writer = new CsvLogWriter(backwards);
            writer.append(sample);
            try {
                writer.append(new LogSample(sample.timeMs() - 1, 1, 1, 1, 1));
                throw new AssertionError("Backwards time accepted");
            } catch (IOException expected) {
                check(CsvLogReader.read(backwards).size() == 1, "Bad timestamp not written");
            }
            writer.close();
            writer.close();
            try {
                writer.append(sample);
                throw new AssertionError("Write after close accepted");
            } catch (IOException expected) {
                check(CsvLogReader.read(backwards).size() == 1, "Closed file unchanged");
            }
        } finally {
            Locale.setDefault(previous);
            try (var paths = Files.list(directory)) {
                for (Path file : paths.toList()) { Files.delete(file); }
            }
            Files.delete(directory);
        }
        System.out.println("C/Java live-data, sensor availability, capability and CSV recording checks passed");
    }
}
