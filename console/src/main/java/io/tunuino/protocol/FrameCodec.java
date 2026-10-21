/* Copyright (c) 2026 Tunuino. */
package io.tunuino.protocol;

import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.Objects;
import java.util.zip.CRC32;

/** Binary framing only; command interpretation belongs to the ECU session. */
public final class FrameCodec {
    public static final int MAX_BODY = 4096;
    private static final int OVERHEAD = 6;

    private FrameCodec() {}

    public static byte[] encode(byte[] body) {
        Objects.requireNonNull(body, "body");
        checkLength(body.length);
        byte[] packet = new byte[body.length + OVERHEAD];
        ByteBuffer buffer = ByteBuffer.wrap(packet);
        buffer.putShort((short) body.length);
        buffer.put(body);
        CRC32 crc = new CRC32();
        crc.update(packet, 2, body.length);
        buffer.putInt((int) crc.getValue());
        return packet;
    }

    public static byte[] decode(byte[] packet) {
        Objects.requireNonNull(packet, "packet");
        if (packet.length < OVERHEAD + 1) {
            throw new IllegalArgumentException("Pacote incompleto");
        }
        ByteBuffer buffer = ByteBuffer.wrap(packet);
        int count = Short.toUnsignedInt(buffer.getShort());
        checkLength(count);
        if (packet.length != count + OVERHEAD) {
            throw new IllegalArgumentException("Comprimento do pacote incorreto");
        }
        CRC32 crc = new CRC32();
        crc.update(packet, 2, count);
        long expected = Integer.toUnsignedLong(buffer.getInt(2 + count));
        if (crc.getValue() != expected) {
            throw new IllegalArgumentException("CRC do pacote incorreto");
        }
        return Arrays.copyOfRange(packet, 2, 2 + count);
    }

    /** The transport must provide its own read deadline and cancellation. */
    public static byte[] readFrom(InputStream input) throws IOException {
        Objects.requireNonNull(input, "input");
        byte[] header = input.readNBytes(2);
        if (header.length != 2) {
            throw new EOFException("Cabeçalho incompleto");
        }
        int count = ((header[0] & 0xff) << 8) | (header[1] & 0xff);
        try {
            checkLength(count);
        } catch (IllegalArgumentException e) {
            throw new IOException(e.getMessage(), e);
        }
        byte[] rest = input.readNBytes(count + 4);
        if (rest.length != count + 4) {
            throw new EOFException("Corpo do pacote incompleto");
        }
        byte[] packet = new byte[count + OVERHEAD];
        System.arraycopy(header, 0, packet, 0, 2);
        System.arraycopy(rest, 0, packet, 2, rest.length);
        try {
            return decode(packet);
        } catch (IllegalArgumentException e) {
            throw new IOException(e.getMessage(), e);
        }
    }

    private static void checkLength(int count) {
        if (count < 1 || count > MAX_BODY) {
            throw new IllegalArgumentException("Corpo deve ter entre 1 e " + MAX_BODY + " bytes");
        }
    }
}
