/* Copyright (c) 2026 Tunuino. */
package io.tunuino;

import io.tunuino.maintenance.*;
import java.nio.file.*;

/** Check upgrade data isolation, legacy settings and cwd-independent firmware discovery. */
public final class AppPathsChecks {
    private static void require(boolean ok, String message) { if (!ok) throw new AssertionError(message); }
    public static void main(String[] args) throws Exception {
        Path work = Files.createTempDirectory("tunuino-user-paths-");
        String home = System.getProperty("user.home");
        String os = System.getProperty("os.name");
        try {
            System.setProperty("user.home", work.toString());
            Path expected = os.startsWith("Windows") ? Path.of(System.getenv("LOCALAPPDATA"), "TunuinoConsole") : work.resolve(".tunuino");
            require(AppPaths.data().equals(expected), "User data path");
            Files.createDirectories(AppPaths.legacyData());
            Path tool = work.resolve("STM32_Programmer_CLI.exe"); Files.writeString(tool, "test only");
            Path legacy = AppPaths.legacyData().resolve("programmers.properties");
            java.util.Properties settings = new java.util.Properties();
            settings.setProperty("STM32_Programmer_CLI.exe", tool.toString());
            try (var writer = Files.newBufferedWriter(legacy)) { settings.store(writer, "legacy check"); }
            ProgrammerTools tools = new ProgrammerTools();
            require(tool.equals(tools.find("STM32_Programmer_CLI.exe")), "Legacy tool selection lost");
            tools.select("STM32_Programmer_CLI.exe", tool);
            require(Files.isRegularFile(expected.resolve("programmers.properties")), "Settings not saved in user directory");
            require(Files.readString(legacy).contains("legacy check") || legacy.equals(expected.resolve("programmers.properties")), "Legacy settings overwritten");
            Path installed = work.resolve("Installed App");
            Path portable = work.resolve("Portable Kit");
            Files.createDirectories(installed.resolve("firmware"));
            Files.writeString(installed.resolve("firmware/manifest.json"), "{}");
            Files.createDirectories(portable.resolve("firmware"));
            Files.writeString(portable.resolve("firmware/manifest.json"), "{}");
            require(AppPaths.firmware(installed.resolve("app"), work).equals(installed.resolve("firmware")), "Installed firmware depends on cwd");
            require(AppPaths.firmware(portable.resolve("console/app"), work).equals(portable.resolve("firmware")), "Portable firmware not found");
            require(AppPaths.firmware(work.resolve("empty/app"), work) == null, "Invented bundled firmware");
            require(!AppPaths.maps().startsWith(installed) && !AppPaths.logs().startsWith(installed)
                && !AppPaths.configurations().startsWith(installed), "User data in application image");
            System.setProperty("os.name", "Windows");
            String local = System.getenv("LOCALAPPDATA");
            if (local != null && !local.isBlank()) require(AppPaths.data().equals(Path.of(local, "TunuinoConsole")), "Windows user data path");
            System.out.println("User data isolation, legacy programmer settings and installed/portable firmware paths passed");
        } finally {
            System.setProperty("user.home", home); System.setProperty("os.name", os);
            try (var files = Files.walk(work)) {
                for (Path path : files.sorted(java.util.Comparator.reverseOrder()).toList()) Files.delete(path);
            }
        }
    }
}
