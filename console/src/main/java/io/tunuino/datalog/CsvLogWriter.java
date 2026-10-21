/* Copyright (c) 2026 Tunuino. */
package io.tunuino.datalog;

import java.io.BufferedWriter;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.Objects;

/** Creates a new log; existing files are never overwritten. */
public final class CsvLogWriter implements AutoCloseable {
    private final BufferedWriter writer;
    private long lastTime = -1;
    private boolean closed;

    public CsvLogWriter(Path path) throws IOException {
        writer = Files.newBufferedWriter(path, StandardCharsets.UTF_8, StandardOpenOption.CREATE_NEW,
            StandardOpenOption.WRITE);
        try {
            writer.write(CsvLogReader.HEADER);
            writer.newLine();
            writer.flush();
        } catch (IOException e) {
            try { writer.close(); } catch (IOException closeFailure) { e.addSuppressed(closeFailure); }
            throw e;
        }
    }

    public synchronized void append(LogSample sample) throws IOException {
        Objects.requireNonNull(sample, "sample");
        if (closed) { throw new IOException("Datalog encerrado"); }
        if (sample.timeMs() < lastTime) { throw new IOException("O relógio da ECU recuou; gravação interrompida"); }
        writer.write(sample.timeMs() + "," + sample.rpm() + "," + sample.mapKpa() + ","
            + sample.lambda() + "," + sample.coolantC());
        writer.newLine();
        writer.flush();
        lastTime = sample.timeMs();
    }

    @Override public synchronized void close() throws IOException {
        if (!closed) {
            closed = true;
            writer.close();
        }
    }
}
