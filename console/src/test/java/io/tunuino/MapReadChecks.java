/* Copyright (c) 2026 Tunuino. */
package io.tunuino;

import io.tunuino.calibration.CalibrationMap;
import io.tunuino.protocol.DeviceIdentity;
import io.tunuino.protocol.FrameCodec;
import io.tunuino.protocol.MapReadClient;
import io.tunuino.transport.PacketSession;
import io.tunuino.transport.SerialChannel;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Duration;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HexFormat;
import java.util.List;
import java.util.zip.CRC32;

public final class MapReadChecks {
    private static void check(boolean ok, String message) { if (!ok) { throw new AssertionError(message); } }
    private static DeviceIdentity identity(long caps) {
        return new DeviceIdentity(DeviceIdentity.Family.PROTEUS, DeviceIdentity.McuSeries.STM32_F4,
            1, 0, 1, 0, caps, "0102030405060708090a0b0c");
    }
    private static final class Channel implements SerialChannel {
        final ByteArrayInputStream responses;
        final ByteArrayOutputStream requests = new ByteArrayOutputStream();
        int closes;
        boolean delay;
        Channel(byte[] bytes) { responses = new ByteArrayInputStream(bytes); }
        public int read(byte[] b, int o, int n, int ms) {
            if (delay) {
                try { Thread.sleep(ms + 2L); } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
            }
            return responses.read(b, o, Math.min(n, 7));
        }
        public int write(byte[] b, int o, int n, int ms) {
            int count = Math.min(n, 3); requests.write(b, o, count); return count;
        }
        public void close() { ++closes; }
    }
    private static List<byte[]> split(byte[] packets) throws IOException {
        List<byte[]> result = new ArrayList<>();
        ByteArrayInputStream in = new ByteArrayInputStream(packets);
        while (in.available() > 0) { result.add(FrameCodec.readFrom(in)); }
        return result;
    }
    private static byte[] join(List<byte[]> bodies) {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        for (byte[] body : bodies) { out.writeBytes(FrameCodec.encode(body)); }
        return out.toByteArray();
    }
    private static void rejected(byte[] packets, CalibrationMap.Kind kind) throws Exception {
        Channel channel = new Channel(packets);
        try (PacketSession session = new PacketSession(channel)) {
            try {
                MapReadClient.read(session, identity(2), kind, Duration.ofSeconds(2));
                throw new AssertionError("Bad map transfer accepted");
            } catch (IOException expected) {
                check(!session.isOpen() && channel.closes == 1, "Failed transfer invalidates session");
            }
        }
    }
    public static void main(String[] args) throws Exception {
        List<String> vectors = Files.readAllLines(Path.of(args[0]));
        for (int k = 0; k < 2; ++k) {
            CalibrationMap.Kind kind = k == 0 ? CalibrationMap.Kind.VE : CalibrationMap.Kind.IGNITION;
            byte[] packets = HexFormat.of().parseHex(vectors.get(k * 2));
            byte[] file = HexFormat.of().parseHex(vectors.get(k * 2 + 1));
            Channel channel = new Channel(packets);
            try (PacketSession session = new PacketSession(channel)) {
                CalibrationMap map = MapReadClient.read(session, identity(2), kind, Duration.ofSeconds(2));
                check(Arrays.equals(map.encode(), file) && map.rows() == 32 && map.columns() == 32, "Full C snapshot read by Java");
                check(session.isOpen() && channel.responses.available() == 0, "Successful read preserves connection");
                List<byte[]> queries = split(channel.requests.toByteArray());
                check(queries.size() == 19, "Metadata plus 18 blocks for map larger than one frame");
                check(Arrays.equals(queries.get(0), ("TUNUINO:M\0" + (char)(k + 1)).getBytes(java.nio.charset.StandardCharsets.US_ASCII)), "Metadata query");
                int offset = 0;
                for (int i = 1; i < queries.size(); ++i) {
                    ByteBuffer q = ByteBuffer.wrap(queries.get(i));
                    int count = Math.min(256, file.length - offset);
                    check(q.capacity() == 19 && q.get(9) == 1 && q.get(10) == k + 1 && q.getInt(11) == 0xFEDCBA98
                        && Short.toUnsignedInt(q.getShort(15)) == offset && Short.toUnsignedInt(q.getShort(17)) == count, "Ordered exact chunk request");
                    offset += count;
                }
            }
            List<byte[]> original = split(packets);
            for (int index : new int[] {0, 1, original.size() - 1}) {
                for (int field : new int[] {0, 1, 2, 3, 4, 8, 10}) {
                    List<byte[]> bad = split(packets);
                    bad.get(index)[field] ^= (byte)0x80;
                    rejected(join(bad), kind);
                }
            }
            for (int size : new int[] {0, 47, 49, 4372, 65535}) {
                List<byte[]> bad = split(packets);
                ByteBuffer.wrap(bad.get(0)).putShort(8, (short)size);
                rejected(join(bad), kind);
            }
            for (int status : new int[] {1, 2, 3}) {
                for (int index : new int[] {0, 2}) {
                    List<byte[]> bad = split(packets);
                    bad.set(index, new byte[] {(byte)status});
                    rejected(join(bad), kind);
                }
            }
            List<byte[]> badCrc = split(packets); badCrc.get(1)[12] ^= 1;
            rejected(join(badCrc), kind); // valid packet CRC, wrong whole-file CRC
            rejected(Arrays.copyOf(packets, packets.length - 1), kind);
            rejected(join(original.subList(0, original.size() - 1)), kind);
            // Correct transfer CRC cannot hide an invalid map file CRC.
            byte[] corruptFile = file.clone(); corruptFile[20] ^= 1;
            CRC32 crc = new CRC32(); crc.update(corruptFile);
            List<byte[]> invalidMap = split(packets);
            ByteBuffer.wrap(invalidMap.get(0)).putInt(10, (int)crc.getValue());
            invalidMap.get(1)[12 + 20] ^= 1;
            rejected(join(invalidMap), kind);
        }
        Channel gated = new Channel(new byte[0]);
        try (PacketSession session = new PacketSession(gated)) {
            try {
                MapReadClient.read(session, identity(0), CalibrationMap.Kind.VE, Duration.ofSeconds(1));
                throw new AssertionError("Missing capability ignored");
            } catch (IllegalStateException expected) { check(session.isOpen() && gated.requests.size() == 0, "Capability checked before I/O"); }
            for (Duration timeout : new Duration[] {Duration.ZERO, Duration.ofSeconds(-1), Duration.ofSeconds(61)}) {
                try {
                    MapReadClient.read(session, identity(2), CalibrationMap.Kind.VE, timeout);
                    throw new AssertionError("Invalid timeout accepted");
                } catch (IllegalArgumentException expected) { check(session.isOpen() && gated.requests.size() == 0, "Timeout checked before I/O"); }
            }
        }
        Channel slow = new Channel(HexFormat.of().parseHex(vectors.get(0))); slow.delay = true;
        try (PacketSession session = new PacketSession(slow)) {
            try {
                MapReadClient.read(session, identity(2), CalibrationMap.Kind.VE, Duration.ofMillis(20));
                throw new AssertionError("Deadline ignored");
            } catch (IOException expected) { check(!session.isOpen(), "Deadline invalidates incomplete transfer"); }
        }
        System.out.println("C/Java maximum VE/ignition reads, fragmented I/O, capability, snapshot, CRC and deadline checks passed");
    }
}
