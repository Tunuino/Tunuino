/* Copyright (c) 2026 Tunuino. */
package io.tunuino.datalog;

import io.tunuino.protocol.DeviceIdentity;
import io.tunuino.protocol.LiveDataClient;
import io.tunuino.transport.PacketSession;
import java.io.IOException;
import java.io.InterruptedIOException;
import java.nio.file.Path;
import java.time.Duration;
import java.util.Objects;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;

public final class LiveDataRecorder {
    private LiveDataRecorder() {}

    /** Run on a worker. A normal stop finishes the current exchange and keeps the link open. */
    public static long record(PacketSession session, DeviceIdentity identity, Path destination,
            Duration interval, BooleanSupplier running, Consumer<LogSample> samples) throws IOException {
        Objects.requireNonNull(session, "session");
        Objects.requireNonNull(identity, "identity");
        Objects.requireNonNull(running, "running");
        Objects.requireNonNull(samples, "samples");
        if ((identity.capabilities() & LiveDataClient.CAPABILITY) == 0) {
            throw new IllegalStateException("O firmware não anuncia dados em tempo real");
        }
        if (interval.compareTo(Duration.ofMillis(10)) < 0 || interval.compareTo(Duration.ofSeconds(5)) > 0) {
            throw new IllegalArgumentException("Intervalo deve estar entre 10 ms e 5 s");
        }
        long count = 0;
        try (CsvLogWriter writer = new CsvLogWriter(destination,(identity.capabilities()&LiveDataClient.PARTIAL_CAPABILITY)!=0)) {
            while (running.getAsBoolean()) {
                var result = LiveDataClient.read(session, identity, Duration.ofMillis(500));
                if (result.isPresent()) {
                    LogSample sample = result.get();
                    writer.append(sample);
                    ++count;
                    samples.accept(sample);
                }
                if (running.getAsBoolean()) {
                    try {
                        long nanos = interval.toNanos();
                        Thread.sleep(nanos / 1_000_000, (int) (nanos % 1_000_000));
                    } catch (InterruptedException e) {
                        Thread.currentThread().interrupt();
                        throw new InterruptedIOException("Gravação cancelada");
                    }
                }
            }
        }
        return count;
    }
}
