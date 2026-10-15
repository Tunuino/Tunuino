package org.tunuino.ui.protocol;

import java.io.IOException;
import java.io.InputStream;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.Consumer;

/** Runs the binary telemetry receiver on a worker thread, never on the Swing event thread. */
public final class TelemetryConnection implements AutoCloseable {
    private final InputStream input;
    private final TelemetryStreamReader reader;
    private final Consumer<Throwable> onError;
    private final AtomicBoolean started = new AtomicBoolean();
    private final Thread worker;

    public TelemetryConnection(InputStream input, Consumer<byte[]> onFrame, Consumer<Throwable> onError) {
        this.input = Objects.requireNonNull(input, "input");
        this.reader = new TelemetryStreamReader(Objects.requireNonNull(onFrame, "onFrame"));
        this.onError = Objects.requireNonNull(onError, "onError");
        this.worker = new Thread(this::receive, "tunuino-telemetry-reader");
        this.worker.setDaemon(true);
    }

    public void start() {
        if (!started.compareAndSet(false, true)) throw new IllegalStateException("Connection already started");
        worker.start();
    }

    private void receive() {
        try {
            reader.readFrom(input);
        } catch (IOException | RuntimeException error) {
            if (!worker.isInterrupted()) onError.accept(error);
        } finally {
            try { input.close(); } catch (IOException ignored) { }
        }
    }

    @Override public void close() throws IOException {
        worker.interrupt();
        input.close();
    }
}
