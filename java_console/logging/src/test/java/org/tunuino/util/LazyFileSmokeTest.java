package org.tunuino.util;

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;

/** Exercises actual file writing and unchanged-content handling. */
public final class LazyFileSmokeTest {
    public static void main(String[] args) throws Exception {
        Path directory = Files.createTempDirectory("tunuino-lazy-file-");
        try {
            Path target = directory.resolve("generated.h");
            try (LazyFile file = new LazyFileImpl(target.toString())) {
                file.write("Tunuino\\n");
            }
            String first = Files.readString(target, StandardCharsets.ISO_8859_1);
            if (!"Tunuino\\n".equals(first)) {
                throw new AssertionError("Unexpected written content: " + first);
            }
            long modified = Files.getLastModifiedTime(target).toMillis();
            try (LazyFile file = new LazyFileImpl(target.toString())) {
                file.write("Tunuino\\n");
            }
            if (Files.getLastModifiedTime(target).toMillis() != modified) {
                throw new AssertionError("Unchanged content was rewritten");
            }
        } finally {
            try (java.util.stream.Stream<Path> paths = Files.walk(directory)) {
                paths.sorted(java.util.Comparator.reverseOrder()).forEach(path -> {
                    try { Files.deleteIfExists(path); }
                    catch (java.io.IOException e) { throw new RuntimeException(e); }
                });
            }
        }
    }
}
