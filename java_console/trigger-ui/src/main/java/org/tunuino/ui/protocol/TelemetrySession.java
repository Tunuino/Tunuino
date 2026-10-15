package org.tunuino.ui.protocol;

import java.io.IOException;
import java.io.InputStream;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.Consumer;

/** Connects a readable telemetry transport to the dashboard and reports its lifecycle. */
public final class TelemetrySession implements AutoCloseable {
    public enum State { DISCONNECTED, CONNECTED, ERROR }

    private final TelemetryConnection connection;
    private final Consumer<State> onState;
    private final AtomicBoolean closed = new AtomicBoolean();

    public TelemetrySession(InputStream input, Consumer<byte[]> onFrame,
                            Consumer<State> onState, Consumer<Throwable> onError) {
        this.onState = Objects.requireNonNull(onState, "onState");
        Objects.requireNonNull(onError, "onError");
        connection = new TelemetryConnection(input, onFrame, error -> {
            if (!closed.get()) {
                onState.accept(State.ERROR);
                onError.accept(error);
            }
        });
    }

    public void start() {
        if (closed.get()) throw new IllegalStateException("Session closed");
        connection.start();
        onState.accept(State.CONNECTED);
    }

    @Override public void close() throws IOException {
        if (closed.compareAndSet(false, true)) {
            try { connection.close(); }
            finally { onState.accept(State.DISCONNECTED); }
        }
    }
}
