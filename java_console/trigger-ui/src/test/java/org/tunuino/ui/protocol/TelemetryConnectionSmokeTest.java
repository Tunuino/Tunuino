package org.tunuino.ui.protocol;

import java.io.ByteArrayInputStream;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

public final class TelemetryConnectionSmokeTest {
    private static byte[] hex(String s) {
        byte[] b = new byte[s.length() / 2];
        for (int i = 0; i < b.length; i++) b[i] = (byte) Integer.parseInt(s.substring(i * 2, i * 2 + 2), 16);
        return b;
    }
    public static void main(String[] args) throws Exception {
        CountDownLatch delivered = new CountDownLatch(1);
        AtomicInteger rpm = new AtomicInteger();
        AtomicReference<Throwable> error = new AtomicReference<>();
        try (TelemetryConnection connection = new TelemetryConnection(
            new ByteArrayInputStream(hex("544e01020e00dc05f5034501e2048a02be0564059dad")),
            frame -> { rpm.set(TelemetryFrame.decode(frame).rpm); delivered.countDown(); }, error::set)) {
            connection.start();
            if (!delivered.await(3, TimeUnit.SECONDS)) throw new AssertionError("No telemetry delivered");
            if (rpm.get() != 1500 || error.get() != null) throw new AssertionError("Invalid telemetry result");
            try {
                connection.start();
                throw new AssertionError("Connection started twice");
            } catch (IllegalStateException expected) { }
        }
    }
}
