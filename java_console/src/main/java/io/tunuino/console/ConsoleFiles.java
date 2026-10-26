/* Copyright (c) 2026 Tunuino. */
package io.tunuino.console;

import java.io.IOException;
import java.nio.file.*;
import javafx.stage.FileChooser;

/** Start save/open dialogs in persistent user folders, never the installation directory. */
final class ConsoleFiles {
    private ConsoleFiles() {}
    static void in(FileChooser chooser, Path directory) {
        if (directory == null) return;
        try {
            Files.createDirectories(directory);
            chooser.setInitialDirectory(directory.toFile());
        } catch (IOException | SecurityException ignored) {
            // The system dialog remains available to select another writable location.
        }
    }
}
