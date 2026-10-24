/* Copyright (c) 2026 Tunuino. */
package io.tunuino.protocol;

import io.tunuino.transport.PacketSession;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.Objects;

public final class IdentityClient {
    private IdentityClient() {}

    public static byte[] request() {
        return "TUNUINO?".getBytes(StandardCharsets.US_ASCII);
    }

    public static DeviceIdentity identify(PacketSession session, DeviceIdentity.Family expected,
            Duration timeout) throws IOException {
        Objects.requireNonNull(session, "session");
        Objects.requireNonNull(expected, "expected");
        Objects.requireNonNull(timeout, "timeout");
        try {
            DeviceIdentity identity = DeviceIdentity.decode(session.exchange(request(), timeout));
            if (identity.family() != expected) {
                throw new IOException("Centralina " + identity.family() + " não corresponde à seleção " + expected);
            }
            return identity;
        } catch (IOException e) {
            try {
                session.close();
            } catch (IOException closeFailure) {
                e.addSuppressed(closeFailure);
            }
            throw e;
        }
    }
}
