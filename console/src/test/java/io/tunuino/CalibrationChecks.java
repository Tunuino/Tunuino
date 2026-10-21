/* Copyright (c) 2026 Tunuino. */
package io.tunuino;

import io.tunuino.calibration.CalibrationMap;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.file.FileAlreadyExistsException;
import java.nio.file.Files;
import java.util.Arrays;
import java.util.HexFormat;
import java.util.zip.CRC32;

public final class CalibrationChecks {
    private static void check(boolean condition, String message) {
        if (!condition) { throw new AssertionError(message); }
    }
    private static void rejected(byte[] file) throws Exception {
        try { CalibrationMap.decode(file); }
        catch (IOException expected) { return; }
        throw new AssertionError("Invalid map was accepted");
    }
    private static void checksum(byte[] file) {
        CRC32 crc = new CRC32(); crc.update(file, 0, file.length - 4);
        ByteBuffer.wrap(file).putInt(file.length - 4, (int)crc.getValue());
    }
    public static void main(String[] args) throws Exception {
        byte[] file = HexFormat.of().parseHex(args[0]);
        CalibrationMap map = CalibrationMap.decode(file);
        check(Arrays.equals(file, map.encode()), "C/Java byte-for-byte file interoperability");
        check(map.kind() == CalibrationMap.Kind.VE && map.rows() == 2 && map.columns() == 2, "Map dimensions and kind");
        for (int rpm = 1000; rpm <= 3000; rpm += 125) {
            for (double load = 50; load <= 100; load += 2.5) {
                var result = map.lookup(rpm, load);
                check(!result.clamped() && Math.abs(result.value() - (-10 + rpm * 0.01 + load * 0.8)) < 1e-9,
                    "Analytic interpolation surface");
            }
        }
        check(map.lookup(99999, 200).clamped() && map.lookup(99999, 200).value() == 100, "Explicit upper clamping");
        check(map.lookup(0, 0).clamped() && map.lookup(0, 0).value() == 40, "Explicit lower clamping");
        for (double input : new double[] {-1, Double.NaN, Double.POSITIVE_INFINITY}) {
            try { map.lookup(input, 75); throw new AssertionError("Invalid lookup input"); }
            catch (IllegalArgumentException expected) { /* rejected */ }
        }
        for (int n = 0; n < file.length; ++n) { rejected(Arrays.copyOf(file, n)); }
        for (int i = 0; i < file.length; ++i) {
            byte[] corrupt = file.clone(); corrupt[i] ^= 1; rejected(corrupt);
        }
        byte[] invalidAxis = file.clone();
        ByteBuffer.wrap(invalidAxis).putInt(16, 1000); checksum(invalidAxis); rejected(invalidAxis);
        byte[] invalidCell = file.clone();
        ByteBuffer.wrap(invalidCell).putInt(28, 300001); checksum(invalidCell); rejected(invalidCell);
        int[] rpm = {1000, 3000}, load = {50000, 100000};
        int[][] values = {{40000, 60000}, {80000, 120000}};
        CalibrationMap curved = new CalibrationMap(CalibrationMap.Kind.VE, rpm, load, values);
        rpm[0] = 0; load[0] = 0; values[0][0] = 0;
        check(curved.rpmAt(0) == 1000 && curved.cell(0, 0) == 40 && curved.lookup(2000, 75).value() == 75,
            "Immutable snapshot and non-planar interpolation");
        byte[] ignition = HexFormat.of().parseHex(args[1]);
        CalibrationMap spark = CalibrationMap.decode(ignition);
        check(spark.kind() == CalibrationMap.Kind.IGNITION && spark.cell(0, 0) == -10
            && spark.lookup(2000, 75).value() == 15 && Arrays.equals(ignition, spark.encode()), "Signed ignition interoperability");
        int[] axis = new int[32]; int[][] cells = new int[32][32];
        for (int i = 0; i < 32; ++i) { axis[i] = i * 1000; Arrays.fill(cells[i], 50000); }
        CalibrationMap maximum = new CalibrationMap(CalibrationMap.Kind.VE, axis, axis, cells);
        check(maximum.encode().length == CalibrationMap.MAX_FILE
            && CalibrationMap.decode(maximum.encode()).lookup(10000, 15).value() == 50, "Maximum dimensions");
        var directory = Files.createTempDirectory("tunuino-map-");
        var path = directory.resolve("test.tnmap");
        try {
            map.writeNew(path);
            check(Arrays.equals(CalibrationMap.read(path).encode(), file), "Disk round trip");
            try { spark.writeNew(path); throw new AssertionError("Existing map overwritten"); }
            catch (FileAlreadyExistsException expected) { check(Arrays.equals(Files.readAllBytes(path), file), "Existing map preserved"); }
            Files.write(path, new byte[CalibrationMap.MAX_FILE + 1]);
            try { CalibrationMap.read(path); throw new AssertionError("Oversized map accepted"); }
            catch (IOException expected) { /* bounded input rejected */ }
        } finally { Files.deleteIfExists(path); Files.delete(directory); }
        System.out.println("C/Java calibration format, interpolation, immutability and file validation checks passed");
    }
}
