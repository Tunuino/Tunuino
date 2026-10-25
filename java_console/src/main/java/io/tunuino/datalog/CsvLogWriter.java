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
    private final boolean partial;

    public CsvLogWriter(Path path) throws IOException {
        this(path,false);
    }
    public CsvLogWriter(Path path, boolean partial) throws IOException {
        this.partial=partial;
        writer = Files.newBufferedWriter(path, StandardCharsets.UTF_8, StandardOpenOption.CREATE_NEW,
            StandardOpenOption.WRITE);
        try {
            writer.write(partial?CsvLogReader.PARTIAL_HEADER:CsvLogReader.HEADER);
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
        if(!partial && sample.validMask()!=15)throw new IOException("Datalog sem máscara de validade não aceita dados parciais");
        StringBuilder row=new StringBuilder(Long.toString(sample.timeMs()));
        double[] values={sample.rpm(),sample.mapKpa(),sample.lambda(),sample.coolantC()};
        for(int i=0;i<4;++i){row.append(',');if((sample.validMask()&(1<<i))!=0)row.append(values[i]);}
        if(partial)row.append(',').append(sample.validMask());
        writer.write(row.toString());
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
