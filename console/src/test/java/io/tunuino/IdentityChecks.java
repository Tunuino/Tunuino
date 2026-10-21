/* Copyright (c) 2026 Tunuino. */
package io.tunuino;

import io.tunuino.protocol.DeviceIdentity;
import io.tunuino.protocol.FrameCodec;
import io.tunuino.protocol.IdentityClient;
import io.tunuino.transport.PacketSession;
import io.tunuino.transport.SerialChannel;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.time.Duration;
import java.util.Arrays;
import java.util.HexFormat;

public final class IdentityChecks {
    private static void check(boolean condition, String message) {
        if (!condition) { throw new AssertionError(message); }
    }

    private static final class Channel implements SerialChannel {
        final byte[] incoming;
        final ByteArrayOutputStream outgoing = new ByteArrayOutputStream();
        int position;
        int closes;
        Channel(byte[] incoming) { this.incoming = incoming; }
        @Override public int read(byte[] b, int offset, int count, int ms) {
            int n = Math.min(3, Math.min(count, incoming.length - position));
            System.arraycopy(incoming, position, b, offset, n);
            position += n;
            return n;
        }
        @Override public int write(byte[] b, int offset, int count, int ms) {
            outgoing.write(b, offset, count);
            return count;
        }
        @Override public void close() { ++closes; }
    }

    private static void rejected(byte[] body, DeviceIdentity.Family expected) throws Exception {
        Channel channel = new Channel(FrameCodec.encode(body));
        try (PacketSession session = new PacketSession(channel)) {
            try {
                IdentityClient.identify(session, expected, Duration.ofSeconds(2));
                throw new AssertionError("Invalid identity accepted");
            } catch (IOException validFailure) {
                check(!session.isOpen() && channel.closes == 1, "Invalid identity closes session");
            }
        }
        check(channel.closes == 1, "Rejected session closed once");
    }

    public static void main(String[] args) throws Exception {
        byte[] cPacket = HexFormat.of().parseHex(args[0]);
        byte[] body = FrameCodec.decode(cPacket);
        DeviceIdentity identity = DeviceIdentity.decode(body);
        check(identity.family() == DeviceIdentity.Family.PROTEUS, "C family");
        check(identity.mcuSeries() == DeviceIdentity.McuSeries.STM32_F4, "C MCU series");
        check(identity.hardwareRevision() == 258, "Network byte order for revision");
        check(identity.firmwareVersion().equals("0.1.3"), "C firmware version");
        check(identity.capabilities() == 0, "No capabilities inferred");
        check(identity.uid().equals("0102030405060708090a0b0c"), "C UID");
        Channel channel = new Channel(cPacket);
        try (PacketSession session = new PacketSession(channel)) {
            check(IdentityClient.identify(session, DeviceIdentity.Family.PROTEUS,
                Duration.ofSeconds(2)).equals(identity), "Full identification transaction");
            check(session.isOpen(), "Valid identity preserves session");
            check(Arrays.equals(channel.outgoing.toByteArray(), FrameCodec.encode(IdentityClient.request())), "Exact query");
        }
        rejected(body, DeviceIdentity.Family.UAEFI);
        for (int offset : new int[] {0, 1, 8, 9, 10}) {
            byte[] modified = body.clone();
            modified[offset] = (byte) 0xFF;
            rejected(modified, DeviceIdentity.Family.PROTEUS);
        }
        byte[] modified = body.clone();
        modified[11] = modified[12] = 0;
        rejected(modified, DeviceIdentity.Family.PROTEUS);
        modified = body.clone();
        modified[16] = (byte) 0x80;
        rejected(modified, DeviceIdentity.Family.PROTEUS);
        for (byte fill : new byte[] {0, (byte) 0xFF}) {
            modified = body.clone();
            Arrays.fill(modified, 20, 32, fill);
            rejected(modified, DeviceIdentity.Family.PROTEUS);
        }
        rejected(Arrays.copyOf(body, 31), DeviceIdentity.Family.PROTEUS);
        rejected(Arrays.copyOf(body, 33), DeviceIdentity.Family.PROTEUS);
        modified = body.clone();
        modified[9] = 2;
        modified[10] = 3;
        check(DeviceIdentity.decode(modified).family() == DeviceIdentity.Family.UAEFI, "uaEFI family");
        check(DeviceIdentity.decode(modified).mcuSeries() == DeviceIdentity.McuSeries.STM32_H7, "H7 series");
        System.out.println("C/Java identity interoperability, family mismatch and malformed identity checks passed");
    }
}
