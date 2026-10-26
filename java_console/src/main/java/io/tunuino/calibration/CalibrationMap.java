/* Copyright (c) 2026 Tunuino. */
package io.tunuino.calibration;

import java.io.IOException;
import java.math.BigDecimal;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.Arrays;
import java.util.zip.CRC32;

/** Immutable standalone map. Reading a file does not write calibration to an ECU. */
public final class CalibrationMap {
    public static final int MAX_AXIS = 32;
    public static final int MAX_FILE = 4368;
    public enum Kind {
        VE(1, "Eficiência volumétrica (%)", 0, 300000),
        IGNITION(2, "Ignição (graus antes do PMS)", -90000, 90000);
        final int id, minimum, maximum;
        final String label;
        Kind(int id, String label, int minimum, int maximum) {
            this.id = id; this.label = label; this.minimum = minimum; this.maximum = maximum;
        }
        @Override public String toString() { return label; }
    }
    public record Lookup(double value, boolean clamped) {}
    private final Kind kind;
    private final int[] rpm, loadMilliKpa;
    private final int[][] cells;

    public CalibrationMap(Kind kind, int[] rpm, int[] loadMilliKpa, int[][] cells) {
        this.kind = java.util.Objects.requireNonNull(kind, "kind");
        this.rpm = rpm.clone(); this.loadMilliKpa = loadMilliKpa.clone();
        validateAxis(this.rpm); validateAxis(this.loadMilliKpa);
        if (cells.length != loadMilliKpa.length) { throw new IllegalArgumentException("Número de linhas inválido"); }
        this.cells = new int[cells.length][];
        for (int j = 0; j < cells.length; ++j) {
            this.cells[j] = cells[j].clone();
            if (this.cells[j].length != rpm.length) { throw new IllegalArgumentException("Número de colunas inválido"); }
            for (int value : this.cells[j]) {
                if (value < kind.minimum || value > kind.maximum) {
                    throw new IllegalArgumentException("Valor fora dos limites do formato de mapa");
                }
            }
        }
    }

    private static void validateAxis(int[] axis) {
        if (axis.length < 2 || axis.length > MAX_AXIS) { throw new IllegalArgumentException("Eixo deve ter 2 a 32 posições"); }
        for (int i = 0; i < axis.length; ++i) {
            if (axis[i] < 0 || (i > 0 && axis[i] <= axis[i - 1])) {
                throw new IllegalArgumentException("Eixo deve ser não negativo e estritamente crescente");
            }
        }
    }

    public Kind kind() { return kind; }
    public int columns() { return rpm.length; }
    public int rows() { return loadMilliKpa.length; }
    public int rpmAt(int column) { return rpm[column]; }
    public double loadKpaAt(int row) { return loadMilliKpa[row] / 1000.0; }
    public double cell(int row, int column) { return cells[row][column] / 1000.0; }

    /** Returns a new validated map. Decimal edits never silently round to file precision. */
    public CalibrationMap withCell(int row, int column, String text) {
        java.util.Objects.checkIndex(row, rows());
        java.util.Objects.checkIndex(column, columns());
        String value = java.util.Objects.requireNonNull(text, "text").trim();
        if (value.length() > 64 || !value.matches("[+-]?(?:[0-9]+(?:\\.[0-9]{0,3})?|\\.[0-9]{1,3})")) {
            throw new IllegalArgumentException("Usa ponto decimal e até três casas decimais");
        }
        int scaled;
        try { scaled = new BigDecimal(value).movePointRight(3).intValueExact(); }
        catch (ArithmeticException e) { throw new IllegalArgumentException("Valor fora dos limites do mapa", e); }
        if (scaled < kind.minimum || scaled > kind.maximum) {
            throw new IllegalArgumentException("Valor deve estar entre " + kind.minimum / 1000.0
                + " e " + kind.maximum / 1000.0);
        }
        if (scaled == cells[row][column]) { return this; }
        int[][] changed = cells.clone();
        changed[row] = cells[row].clone();
        changed[row][column] = scaled;
        return new CalibrationMap(kind, rpm, loadMilliKpa, changed);
    }

    public Lookup lookup(double speed, double loadKpa) {
        if (!Double.isFinite(speed) || !Double.isFinite(loadKpa) || speed < 0 || loadKpa < 0) {
            throw new IllegalArgumentException("RPM e MAP devem ser finitos e não negativos");
        }
        double ymin = loadKpaAt(0), ymax = loadKpaAt(rows() - 1);
        boolean clamped = speed < rpm[0] || speed > rpm[columns() - 1] || loadKpa < ymin || loadKpa > ymax;
        double x = Math.max(rpm[0], Math.min(rpm[columns() - 1], speed));
        double y = loadKpa <= ymin ? loadMilliKpa[0] : loadKpa >= ymax ? loadMilliKpa[rows() - 1] : loadKpa * 1000;
        int i = bin(rpm, x), j = bin(loadMilliKpa, y);
        double wx = (x - rpm[i]) / ((double) rpm[i + 1] - rpm[i]);
        double wy = (y - loadMilliKpa[j]) / ((double) loadMilliKpa[j + 1] - loadMilliKpa[j]);
        double low = cells[j][i] * (1 - wx) + cells[j][i + 1] * wx;
        double high = cells[j + 1][i] * (1 - wx) + cells[j + 1][i + 1] * wx;
        return new Lookup((low * (1 - wy) + high * wy) / 1000, clamped);
    }
    private static int bin(int[] axis, double value) {
        int i = 0;
        while (i + 2 < axis.length && value > axis[i + 1]) { ++i; }
        return i;
    }

    /**
     * Create a 16×16 offline map without discarding any original 8×8 axis
     * knot or the cell at the intersection of two original knots.
     * Intermediate cells use bilinear interpolation rounded to 0.001.
     * The resulting file cannot be uploaded to the current 8×8 ECU profile.
     */
    public CalibrationMap refineTo16() {
        if (rows() != 8 || columns() != 8)
            throw new IllegalArgumentException("A conversão exige um mapa 8 × 8");
        int[] fineRpm = refineAxis16(rpm), fineLoad = refineAxis16(loadMilliKpa);
        int[][] interpolated = new int[16][16];
        for (int row = 0; row < 16; ++row) {
            for (int col = 0; col < 16; ++col) {
                int originalRow = Arrays.binarySearch(loadMilliKpa, fineLoad[row]);
                int originalCol = Arrays.binarySearch(rpm, fineRpm[col]);
                if (originalRow >= 0 && originalCol >= 0) {
                    interpolated[row][col] = cells[originalRow][originalCol];
                } else {
                    double value = lookup(fineRpm[col], fineLoad[row] / 1000.0).value();
                    // C lround() uses ties away from zero, including negative spark.
                    double milli=value*1000.0;
                    interpolated[row][col] = Math.toIntExact(
                        milli>=0 ? Math.round(milli) : -Math.round(-milli));
                }
            }
        }
        return new CalibrationMap(kind, fineRpm, fineLoad, interpolated);
    }

    private static int[] refineAxis16(int[] original) {
        int[] refined = Arrays.copyOf(original, 16);
        for (int size = 8; size < 16; ++size) {
            int best = -1;
            long widest = 0;
            for (int i = 0; i < size - 1; ++i) {
                long gap = (long)refined[i + 1] - refined[i];
                if (gap > widest) { widest = gap; best = i; }
            }
            if (widest < 2)
                throw new IllegalArgumentException("O eixo não tem espaço entre pontos para criar 16 posições únicas");
            int middle = Math.toIntExact((long)refined[best] + widest / 2);
            System.arraycopy(refined, best + 1, refined, best + 2, size - best - 1);
            refined[best + 1] = middle;
        }
        return refined;
    }

    public byte[] encode() {
        ByteBuffer b = ByteBuffer.allocate(16 + 4 * (columns() + rows() + columns() * rows()));
        b.put("TUNUINOM".getBytes(StandardCharsets.US_ASCII));
        b.put((byte) 1).put((byte) kind.id).put((byte) columns()).put((byte) rows());
        for (int value : rpm) { b.putInt(value); }
        for (int value : loadMilliKpa) { b.putInt(value); }
        for (int[] row : cells) { for (int value : row) { b.putInt(value); } }
        CRC32 crc = new CRC32(); crc.update(b.array(), 0, b.position());
        b.putInt((int) crc.getValue());
        return b.array();
    }

    public static CalibrationMap decode(byte[] file) throws IOException {
        if (file == null || file.length < 48 || file.length > MAX_FILE
                || !Arrays.equals(Arrays.copyOf(file, 8), "TUNUINOM".getBytes(StandardCharsets.US_ASCII))
                || file[8] != 1) { throw new IOException("Formato de mapa inválido"); }
        int nx = file[10] & 255, ny = file[11] & 255;
        if (nx < 2 || nx > MAX_AXIS || ny < 2 || ny > MAX_AXIS
                || file.length != 16 + 4 * (nx + ny + nx * ny)) { throw new IOException("Dimensões do mapa inválidas"); }
        Kind kind = switch (file[9]) {
            case 1 -> Kind.VE; case 2 -> Kind.IGNITION;
            default -> throw new IOException("Tipo de mapa desconhecido");
        };
        ByteBuffer b = ByteBuffer.wrap(file);
        CRC32 crc = new CRC32(); crc.update(file, 0, file.length - 4);
        if (crc.getValue() != Integer.toUnsignedLong(b.getInt(file.length - 4))) { throw new IOException("CRC do mapa incorreto"); }
        b.position(12);
        int[] rpm = new int[nx], load = new int[ny]; int[][] cells = new int[ny][nx];
        for (int i = 0; i < nx; ++i) { rpm[i] = b.getInt(); }
        for (int j = 0; j < ny; ++j) { load[j] = b.getInt(); }
        for (int j = 0; j < ny; ++j) { for (int i = 0; i < nx; ++i) { cells[j][i] = b.getInt(); } }
        try { return new CalibrationMap(kind, rpm, load, cells); }
        catch (IllegalArgumentException e) { throw new IOException(e.getMessage(), e); }
    }

    public static CalibrationMap read(Path file) throws IOException {
        try (var input = Files.newInputStream(file)) { return decode(input.readNBytes(MAX_FILE + 1)); }
    }
    public void writeNew(Path file) throws IOException {
        Files.write(file, encode(), StandardOpenOption.CREATE_NEW, StandardOpenOption.WRITE);
    }
}
