/* Copyright (c) 2026 Tunuino. */
package io.tunuino.android;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;

public final class BenchProtocolChecks {
    private BenchProtocolChecks() {}
    private interface Attempt { void run() throws IOException; }
    private static void rejected(Attempt action) throws IOException {
        try { action.run(); } catch (IOException expected) { return; }
        throw new AssertionError("Invalid response was accepted");
    }
    private static void check(boolean condition) { if (!condition) throw new AssertionError(); }

    private static final class Fake implements BenchProtocol.Transport {
        final byte[] input;
        final ByteArrayOutputStream sent = new ByteArrayOutputStream();
        final int fragment;
        int offset;
        Fake(byte[] input, int fragment) { this.input = input; this.fragment = fragment; }
        @Override public int read(byte[] out, int start, int count, int timeout) {
            check(timeout > 0 && timeout <= 2000);
            int size = Math.min(Math.min(count, fragment), input.length - offset);
            System.arraycopy(input, offset, out, start, size); offset += size; return size;
        }
        @Override public int write(byte[] out, int start, int count, int timeout) {
            check(timeout > 0 && timeout <= 2000);
            int size = Math.min(count, fragment); sent.write(out, start, size); return size;
        }
    }

    public static void main(String[] args) throws IOException {
        byte[] body = new byte[32];
        System.arraycopy("TUNUINO".getBytes(StandardCharsets.US_ASCII), 0, body, 1, 7);
        body[8] = body[9] = body[10] = 1; body[12] = 7; body[15] = 3;
        for (int i = 0; i < 12; i++) body[20 + i] = (byte) (i + 1);
        byte[] packet = BenchProtocol.encode(body);
        if (args.length > 0) {
            byte[] fromC = new byte[args[0].length() / 2];
            for (int i = 0; i < fromC.length; i++) fromC[i] = (byte) Integer.parseInt(args[0].substring(i * 2, i * 2 + 2), 16);
            check(Arrays.equals(packet, fromC));
            packet = fromC;
        }
        for (int size : new int[] {1, 2, 3, 7, 64}) {
            Fake transport = new Fake(packet, size);
            BenchProtocol.Identity identity = BenchProtocol.identify(transport);
            check(identity.family.equals("Proteus") && identity.mcu.equals("STM32 F4") && identity.revision == 7);
            check(identity.version.equals("0.0.3") && identity.uid.equals("0102030405060708090a0b0c") && identity.capabilities == 0);
            check(Arrays.equals(BenchProtocol.decode(transport.sent.toByteArray()), "TUNUINO?".getBytes(StandardCharsets.US_ASCII)));
        }
        byte[] badCrc = packet.clone(); badCrc[10] ^= 1;
        rejected(() -> BenchProtocol.decode(badCrc));
        rejected(() -> BenchProtocol.identify(new Fake(new byte[] {0, 0}, 1)));
        rejected(() -> BenchProtocol.identify(new Fake(new byte[] {0x10, 1}, 1)));
        rejected(() -> BenchProtocol.identify(new Fake(new byte[0], 1)));
        rejected(() -> BenchProtocol.Identity.decode(new byte[32]));
        byte[] zeroUid = body.clone(); Arrays.fill(zeroUid, 20, 32, (byte) 0);
        rejected(() -> BenchProtocol.Identity.decode(zeroUid));
        byte[] invalidFamily = body.clone(); invalidFamily[9] = 9;
        rejected(() -> BenchProtocol.Identity.decode(invalidFamily));
        byte[] invalidCaps = body.clone(); invalidCaps[19] = 32;
        rejected(() -> BenchProtocol.Identity.decode(invalidCaps));
        Thread.currentThread().interrupt();
        try { rejected(() -> BenchProtocol.identify(new Fake(new byte[0], 1))); }
        finally { Thread.interrupted(); }
        System.out.println("Android protocol: C identity, CRC, fragmented reads/writes, invalid UID/family/capabilities, timeout and cancellation passed.");
    }
}
