/* Copyright (c) 2026 Tunuino. */
package io.tunuino;

import io.tunuino.datalog.CsvLogReader;
import io.tunuino.protocol.FrameCodec;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.HexFormat;
import java.util.Random;

public final class CoreChecks {
    private static void check(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError(message);
        }
    }

    private static void invalid(Runnable operation) {
        try {
            operation.run();
        } catch (IllegalArgumentException expected) {
            return;
        }
        throw new AssertionError("Invalid input was accepted");
    }

    private static void badCsv(Path file, String content) throws IOException {
        Files.writeString(file, content);
        try {
            CsvLogReader.read(file);
        } catch (IOException expected) {
            return;
        }
        throw new AssertionError("Invalid CSV was accepted");
    }

    public static void main(String[] args) throws Exception {
        byte[] canonical = "123456789".getBytes(StandardCharsets.US_ASCII);
        byte[] packet = FrameCodec.encode(canonical);
        String hex = HexFormat.of().formatHex(packet);
        check(hex.equals("0009313233343536373839cbf43926"), "CRC reference vector");
        check(hex.equals(args[0]), "C and Java encoders disagree");
        Random random = new Random(20261021);
        for (int length : new int[] {1, 2, 255, 256, 4095, 4096}) {
            byte[] body = new byte[length];
            random.nextBytes(body);
            byte[] encoded = FrameCodec.encode(body);
            check(Arrays.equals(body, FrameCodec.decode(encoded)), "Round trip");
            check(Arrays.equals(body, FrameCodec.readFrom(new ByteArrayInputStream(encoded))), "Stream read");
            encoded[2] ^= 1;
            invalid(() -> FrameCodec.decode(encoded));
        }
        for (int bit = 0; bit < packet.length * 8; ++bit) {
            byte[] corrupt = packet.clone();
            corrupt[bit / 8] ^= (byte) (1 << (bit % 8));
            invalid(() -> FrameCodec.decode(corrupt));
        }
        invalid(() -> FrameCodec.encode(new byte[0]));
        invalid(() -> FrameCodec.encode(new byte[4097]));
        for (int n = 0; n < packet.length; ++n) {
            byte[] truncated = Arrays.copyOf(packet, n);
            invalid(() -> FrameCodec.decode(truncated));
            try {
                FrameCodec.readFrom(new ByteArrayInputStream(truncated));
                throw new AssertionError("Truncated stream was accepted");
            } catch (IOException expected) {
                // A truncated stream must not be treated as a valid response.
            }
        }
        byte[] joined = new byte[packet.length * 2];
        System.arraycopy(packet, 0, joined, 0, packet.length);
        System.arraycopy(packet, 0, joined, packet.length, packet.length);
        ByteArrayInputStream stream = new ByteArrayInputStream(joined);
        check(Arrays.equals(canonical, FrameCodec.readFrom(stream)), "First packet");
        check(Arrays.equals(canonical, FrameCodec.readFrom(stream)), "Second packet");
        check(stream.available() == 0, "Consumed both packets exactly");

        Path csv = Files.createTempFile("tunuino-test-", ".csv");
        String header = CsvLogReader.HEADER + "\n";
        try {
            Files.writeString(csv, "\uFEFF" + header + "0,0,100,1,20\n1000,900,45,1,22\n");
            var samples = CsvLogReader.read(csv);
            check(samples.size() == 2 && samples.get(1).rpm() == 900, "Valid datalog");
            badCsv(csv, header);
            badCsv(csv, "wrong\n0,1,2,3,4\n");
            badCsv(csv, header + "1,NaN,1,1,1\n");
            badCsv(csv, header + "1,1,Infinity,1,1\n");
            badCsv(csv, header + "1,-1,1,1,1\n");
            badCsv(csv, header + "2,1,1,1,1\n1,1,1,1,1\n");
            badCsv(csv, header + "0,1,2,3\n");
            badCsv(csv, header + "0," + "1".repeat(5000) + ",1,1,1\n");
        } finally {
            Files.deleteIfExists(csv);
        }
        System.out.println(hex);
        System.out.println("Java protocol and offline CSV checks passed");
    }
}
