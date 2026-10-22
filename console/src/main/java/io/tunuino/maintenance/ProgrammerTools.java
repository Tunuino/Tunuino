/* Copyright (c) 2026 Tunuino. */
package io.tunuino.maintenance;

import java.io.*;
import java.nio.file.*;
import java.util.*;

/** Discover installed ST tooling or tools beside the portable app; persist explicit selections. */
public final class ProgrammerTools {
    private final Properties settings = new Properties();
    private final Path settingsFile = Path.of(System.getProperty("user.home"), ".tunuino", "programmers.properties");
    private final Path appRoot;
    public ProgrammerTools() {
        Path root;
        try { root = Path.of(ProgrammerTools.class.getProtectionDomain().getCodeSource().getLocation().toURI()).getParent(); }
        catch (Exception e) { root = Path.of(".").toAbsolutePath(); }
        appRoot = root;
        try (Reader reader = Files.newBufferedReader(settingsFile)) { settings.load(reader); }
        catch (IOException ignored) { /* First run uses discovery. */ }
    }
    public Path find(String name) {
        String selected = settings.getProperty(name);
        if (selected != null && Files.isRegularFile(Path.of(selected))) return Path.of(selected);
        List<Path> candidates = new ArrayList<>();
        for (Path root : List.of(appRoot, appRoot.resolve(".."), Path.of(".").toAbsolutePath())) {
            candidates.add(root.resolve("tools").resolve(name));
            candidates.add(root.resolve("tools/openblt").resolve(name));
            candidates.add(root.resolve("tools/STM32_Programmer_CLI/bin").resolve(name));
            candidates.add(root.resolve("STM32_Programmer_CLI/bin").resolve(name));
            candidates.add(root.resolve("drivers").resolve(name));
            candidates.add(root.resolve(name));
        }
        if (name.equals("STM32_Programmer_CLI.exe")) {
            for (String env : List.of("ProgramW6432", "ProgramFiles", "ProgramFiles(x86)")) {
                String value = System.getenv(env);
                if (value != null) candidates.add(Path.of(value, "STMicroelectronics", "STM32Cube", "STM32CubeProgrammer", "bin", name));
            }
        }
        return candidates.stream().map(Path::normalize).filter(Files::isRegularFile).findFirst().orElse(null);
    }
    public void select(String name, Path path) throws IOException {
        if (!path.getFileName().toString().equalsIgnoreCase(name) || !Files.isRegularFile(path))
            throw new IOException("Seleciona " + name);
        settings.setProperty(name, path.toAbsolutePath().toString());
        Files.createDirectories(settingsFile.getParent());
        try (Writer writer = Files.newBufferedWriter(settingsFile)) { settings.store(writer, "Tunuino programmer paths"); }
    }
}
