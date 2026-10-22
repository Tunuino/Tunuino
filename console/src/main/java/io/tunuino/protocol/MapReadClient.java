/* Copyright (c) 2026 Tunuino. */
package io.tunuino.protocol;

import io.tunuino.calibration.CalibrationMap;
import io.tunuino.transport.PacketSession;
import java.io.IOException;
import java.net.SocketTimeoutException;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.Objects;
import java.util.zip.CRC32;

/** Bounded, read-only transfer. No partial map is exposed to callers. */
public final class MapReadClient {
    public static final long CAPABILITY = 2;
    private static final int CHUNK = 256;
    private MapReadClient() {}

    private static ByteBuffer query(int op, int kind) {
        ByteBuffer b = ByteBuffer.allocate(op == 0 ? 11 : 19);
        b.put("TUNUINO:M".getBytes(StandardCharsets.US_ASCII));
        return b.put((byte)op).put((byte)kind);
    }
    private static Duration remaining(long deadline) throws SocketTimeoutException {
        long nanos = deadline - System.nanoTime();
        if (nanos <= 0) { throw new SocketTimeoutException("Tempo total de leitura do mapa excedido"); }
        return Duration.ofNanos(nanos);
    }
    private static ByteBuffer response(byte[] body, int op, int kind, int size) throws IOException {
        if (body.length == 1 && body[0] == 1) { throw new IOException("Mapa indisponível na ECU"); }
        if (body.length == 1 && body[0] == 2) { throw new IOException("O mapa mudou durante a leitura; volta a ler"); }
        if (body.length != size || body[0] != 0 || body[1] != 1 || body[2] != op || body[3] != kind) {
            throw new IOException("Resposta de leitura do mapa inválida");
        }
        return ByteBuffer.wrap(body);
    }
    public static CalibrationMap read(PacketSession session, DeviceIdentity identity,
            CalibrationMap.Kind kind, Duration timeout) throws IOException {
        Objects.requireNonNull(session, "session");
        Objects.requireNonNull(identity, "identity");
        Objects.requireNonNull(kind, "kind");
        Objects.requireNonNull(timeout, "timeout");
        if ((identity.capabilities() & CAPABILITY) == 0) {
            throw new IllegalStateException("O firmware não anuncia leitura de mapas");
        }
        if (timeout.isNegative() || timeout.isZero() || timeout.compareTo(Duration.ofSeconds(60)) > 0) {
            throw new IllegalArgumentException("Timeout deve estar entre 1 ns e 60 s");
        }
        int id = kind == CalibrationMap.Kind.VE ? 1 : 2;
        long deadline = System.nanoTime() + timeout.toNanos();
        try {
            ByteBuffer meta = response(session.exchange(query(0, id).array(), remaining(deadline)), 0, id, 14);
            int generation = meta.getInt(4);
            int length = Short.toUnsignedInt(meta.getShort(8));
            long crc = Integer.toUnsignedLong(meta.getInt(10));
            if (generation == 0 || length < 48 || length > CalibrationMap.MAX_FILE || length % 4 != 0) {
                throw new IOException("Metadados do mapa inválidos");
            }
            byte[] file = new byte[length];
            for (int offset = 0; offset < length;) {
                int count = Math.min(CHUNK, length - offset);
                byte[] request = query(1, id).putInt(generation).putShort((short)offset).putShort((short)count).array();
                ByteBuffer block = response(session.exchange(request, remaining(deadline)), 1, id, 12 + count);
                if (block.getInt(4) != generation || Short.toUnsignedInt(block.getShort(8)) != offset
                        || Short.toUnsignedInt(block.getShort(10)) != count) {
                    throw new IOException("Bloco do mapa fora de sequência");
                }
                System.arraycopy(block.array(), 12, file, offset, count);
                offset += count;
            }
            remaining(deadline);
            CRC32 checksum = new CRC32();
            checksum.update(file);
            if (checksum.getValue() != crc) { throw new IOException("CRC da transferência do mapa inválido"); }
            CalibrationMap map = CalibrationMap.decode(file);
            if (map.kind() != kind) { throw new IOException("Tipo de mapa diferente do pedido"); }
            remaining(deadline);
            return map;
        } catch (IOException e) {
            try { session.close(); } catch (IOException closeFailure) { e.addSuppressed(closeFailure); }
            throw e;
        }
    }
}
