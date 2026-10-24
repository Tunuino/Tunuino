/* Copyright (c) 2026 Tunuino. */
package io.tunuino.tools;

import io.tunuino.calibration.CalibrationMap;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.PrintStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;

public final class ToolChecks {
    private ToolChecks() {}
    private static void check(boolean value) { if (!value) throw new AssertionError(); }
    public static void main(String[] args) throws Exception {
        Path work = Files.createTempDirectory("tunuino-tools-");
        try {
            for (var kind : CalibrationMap.Kind.values()) {
                CalibrationMap map = new CalibrationMap(kind,new int[]{0,1000,6000},new int[]{20000,100000},
                    new int[][]{{12345,23456,34567},{45678,56789,67890}});
                Path source = work.resolve(kind.name()+".tnmap"), csv = work.resolve(kind.name()+".csv"), result = work.resolve(kind.name()+"-back.tnmap");
                map.writeNew(source); Tools.exportMap(source,csv); Tools.importMap(csv,result);
                check(Arrays.equals(map.encode(),CalibrationMap.read(result).encode()));
                try { Tools.exportMap(source,csv); throw new AssertionError("Overwrote an existing CSV"); }
                catch (IOException expected) { check(Files.readString(csv).contains("12.345")); }
                String original = Files.readString(csv);
                Files.writeString(csv,original.replace("12.345","12.3456"));
                try { Tools.importMap(csv,work.resolve("invalid.tnmap")); throw new AssertionError("Silently rounded CSV"); }
                catch (IllegalArgumentException expected) { check(!Files.exists(work.resolve("invalid.tnmap"))); }
            }
            Path input = work.resolve("input.bin"), output = work.resolve("embedded.h");
            Files.write(input,new byte[]{0,1,127,(byte)128,(byte)255});
            Tools.bin2header(input,output,"fixture");
            check(Files.readString(output).contains("0x00, 0x01, 0x7f, 0x80, 0xff,"));
            try { Tools.bin2header(input,work.resolve("bad.h"),"if"); throw new AssertionError("C keyword accepted"); }
            catch (IllegalArgumentException expected) { check(!Files.exists(work.resolve("bad.h"))); }
            ByteArrayOutputStream text = new ByteArrayOutputStream();
            check(Tools.run(new String[]{"--help"},new PrintStream(text),System.err)==0 && text.toString().contains("simulate"));
            if (args.length == 1) SimulatorChecks.run(Path.of(args[0]),work);
            System.out.println("Java tools: lossless VE/ignition CSV roundtrips, exact decimals, file preservation, C-header bytes and CLI checks passed.");
        } finally {
            try (var paths = Files.walk(work)) { for (Path path : paths.sorted(java.util.Comparator.reverseOrder()).toList()) Files.delete(path); }
        }
    }
}
