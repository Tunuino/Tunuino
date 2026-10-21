/* Copyright (c) 2026 Tunuino. */
package io.tunuino.datalog;

import java.io.BufferedReader;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CancellationException;

public final class CsvLogReader {
    public static final String HEADER = "time_ms,rpm,map_kpa,lambda,coolant_c";
    public static final int MAX_SAMPLES = 500_000;
    private static final int MAX_LINE = 4096;

    private CsvLogReader() {}

    public static List<LogSample> read(Path file) throws IOException {
        List<LogSample> samples = new ArrayList<>();
        try (BufferedReader reader = Files.newBufferedReader(file, StandardCharsets.UTF_8)) {
            String header = readLine(reader);
            if (header != null && header.startsWith("\uFEFF")) {
                header = header.substring(1);
            }
            if (!HEADER.equals(header)) {
                throw new IOException("Cabeçalho esperado: " + HEADER);
            }
            long previousTime = -1;
            String line;
            int lineNumber = 1;
            while ((line = readLine(reader)) != null) {
                ++lineNumber;
                if (Thread.currentThread().isInterrupted()) {
                    throw new CancellationException("Leitura cancelada");
                }
                if (line.isBlank()) {
                    continue;
                }
                if (samples.size() == MAX_SAMPLES) {
                    throw new IOException("Limite de " + MAX_SAMPLES + " amostras excedido");
                }
                String[] fields = line.split(",", -1);
                try {
                    if (fields.length != 5) {
                        throw new IllegalArgumentException("São necessárias cinco colunas");
                    }
                    LogSample sample = new LogSample(Long.parseLong(fields[0].trim()),
                        Double.parseDouble(fields[1].trim()), Double.parseDouble(fields[2].trim()),
                        Double.parseDouble(fields[3].trim()), Double.parseDouble(fields[4].trim()));
                    if (sample.timeMs() < previousTime) {
                        throw new IllegalArgumentException("Tempo fora de ordem");
                    }
                    samples.add(sample);
                    previousTime = sample.timeMs();
                } catch (IllegalArgumentException e) {
                    throw new IOException("Linha " + lineNumber + ": " + e.getMessage(), e);
                }
            }
        }
        if (samples.isEmpty()) {
            throw new IOException("O ficheiro não contém amostras");
        }
        return List.copyOf(samples);
    }

    private static String readLine(BufferedReader reader) throws IOException {
        StringBuilder line = new StringBuilder();
        int c;
        while ((c = reader.read()) != -1) {
            if (c == '\n') {
                break;
            }
            if (c == '\r') {
                reader.mark(1);
                if (reader.read() != '\n') {
                    reader.reset();
                }
                break;
            }
            if (line.length() >= MAX_LINE) {
                throw new IOException("Linha excede " + MAX_LINE + " caracteres");
            }
            line.append((char) c);
        }
        return c == -1 && line.isEmpty() ? null : line.toString();
    }
}
