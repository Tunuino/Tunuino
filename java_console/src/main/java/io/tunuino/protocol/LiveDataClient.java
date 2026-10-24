/* Copyright (c) 2026 Tunuino. */
package io.tunuino.protocol;

import io.tunuino.datalog.LogSample;
import io.tunuino.transport.PacketSession;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.Optional;

public final class LiveDataClient {
    public static final long CAPABILITY = 1;
    private LiveDataClient() {}

    public static byte[] request() {
        return "TUNUINO:L".getBytes(StandardCharsets.US_ASCII);
    }

    public static Optional<LogSample> decode(byte[] body) throws IOException {
        if (body != null && body.length == 1 && body[0] == 1) {
            return Optional.empty();
        }
        if (body == null || body.length != 28 || body[0] != 0 || body[1] != 1
                || body[2] != 15 || body[3] != 0) {
            throw new IOException("Resposta de dados em tempo real inválida");
        }
        ByteBuffer b = ByteBuffer.wrap(body);
        try {
            return Optional.of(new LogSample(b.getLong(4),
                Integer.toUnsignedLong(b.getInt(12)) / 1000.0,
                Integer.toUnsignedLong(b.getInt(16)) / 1000.0,
                Integer.toUnsignedLong(b.getInt(20)) / 1_000_000.0,
                b.getInt(24) / 1000.0));
        } catch (IllegalArgumentException e) {
            throw new IOException(e.getMessage(), e);
        }
    }

    public static Optional<LogSample> read(PacketSession session, DeviceIdentity identity,
            Duration timeout) throws IOException {
        if ((identity.capabilities() & CAPABILITY) == 0) {
            throw new IllegalStateException("O firmware não anuncia dados em tempo real");
        }
        try {
            return decode(session.exchange(request(), timeout));
        } catch (IOException e) {
            try { session.close(); } catch (IOException closeFailure) { e.addSuppressed(closeFailure); }
            throw e;
        }
    }
}
