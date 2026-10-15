package org.tunuino.tools;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.charset.StandardCharsets;

/** Verifies actual conversion of binary data into a C header. */
public final class Bin2HeaderSmokeTest {
    public static void main(String[] args) throws Exception {
        Path dir = Files.createTempDirectory("tunuino-bin2header-");
        try {
            Path input = dir.resolve("sample.bin");
            Path output = dir.resolve("sample.h");
            Files.write(input, new byte[] { 0, 1, 15, 16, (byte) 255 });
            Bin2Header.main(new String[] { input.toString(), output.toString(), "const unsigned char sample[]" });
            String text = Files.readString(output, StandardCharsets.UTF_8);
            for (String expected : new String[] { "#ifndef SAMPLE_H_", "#define SAMPLE_H_", "const unsigned char sample[] = {", "0x00", "0x01", "0x0f", "0x10", "0xff", "#endif" }) {
                if (!text.contains(expected)) {
                    throw new AssertionError("Missing generated header fragment: " + expected);
                }
            }
        } finally {
            try (java.util.stream.Stream<Path> paths = Files.walk(dir)) {
                paths.sorted(java.util.Comparator.reverseOrder()).forEach(path -> {
                    try { Files.deleteIfExists(path); }
                    catch (java.io.IOException e) { throw new RuntimeException(e); }
                });
            }
        }
    }
}
