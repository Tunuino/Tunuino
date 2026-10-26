/* Copyright (c) 2026 Tunuino. */
package io.tunuino.maintenance;

import java.nio.file.*;
import java.util.List;

/** Writable user data is deliberately outside the installer-owned application image. */
public final class AppPaths {
    private AppPaths() {}

    public static Path data() {
        String local = System.getenv("LOCALAPPDATA");
        return System.getProperty("os.name", "").startsWith("Windows") && local != null && !local.isBlank()
            ? Path.of(local, "TunuinoConsole") : legacyData();
    }
    public static Path legacyData() { return Path.of(System.getProperty("user.home"), ".tunuino"); }
    public static Path maps() { return data().resolve("maps"); }
    public static Path configurations() { return data().resolve("configurations"); }
    public static Path logs() { return data().resolve("logs"); }

    public static Path firmware() {
        Path app;
        try { app = Path.of(AppPaths.class.getProtectionDomain().getCodeSource().getLocation().toURI()).getParent(); }
        catch (Exception error) { app = Path.of(".").toAbsolutePath(); }
        return firmware(app, Path.of(".").toAbsolutePath());
    }
    public static Path firmware(Path app, Path workingDirectory) {
        for (Path root : List.of(app, app.resolve(".."), app.resolve("../.."), workingDirectory)) {
            Path candidate = root.resolve("firmware").toAbsolutePath().normalize();
            // Avoid selecting a source checkout's firmware directory.
            if (Files.isRegularFile(candidate.resolve("manifest.json"))) return candidate;
        }
        return null;
    }
}
