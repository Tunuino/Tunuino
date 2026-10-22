/* Copyright (c) 2026 Tunuino. */
package io.tunuino.maintenance;

import java.io.IOException;
import java.nio.file.Path;
import java.time.Duration;
import java.util.*;
import java.util.regex.*;
import java.util.function.Consumer;

/** DFU is a USB device, not a COM port. Query hardware IDs, not translated captions. */
public final class WindowsDfu {
    private WindowsDfu() {}
    public record Device(String instanceId, int driverError, String name) {
        @Override public String toString() {
            return name + (driverError == 0 ? " · driver OK" : " · erro de driver " + driverError);
        }
    }
    public record UsbPort(String port, String serial) {
        @Override public String toString() { return port + " · " + serial; }
    }
    private static final String QUERY = "$ErrorActionPreference='Stop'; "
        + "Get-CimInstance Win32_PnPEntity | Where-Object { $_.Present -eq $true -and "
        + "$_.PNPDeviceID -like 'USB\\VID_0483&PID_DF11*' } | ForEach-Object { "
        + "'{0}|{1}|{2}' -f $_.PNPDeviceID,$_.ConfigManagerErrorCode,$_.Name }";

    public static List<Device> scan() throws IOException {
        CommandRunner.Result r = CommandRunner.run(List.of("powershell.exe", "-NoProfile", "-NonInteractive",
            "-Command", QUERY), null, Duration.ofSeconds(15), ignored -> {});
        if (r.exitCode() != 0) throw new IOException("Falha na pesquisa DFU: " + r.output());
        return parseDevices(r.output());
    }
    public static List<Device> parseDevices(String output) throws IOException {
        List<Device> result = new ArrayList<>();
        for (String line : output.split("\\R")) {
            if (line.isBlank()) continue;
            String[] fields = line.strip().split("\\|", 3);
            if (fields.length != 3 || !fields[0].toUpperCase(Locale.ROOT).startsWith("USB\\VID_0483&PID_DF11"))
                throw new IOException("Resposta de pesquisa DFU inválida");
            try { result.add(new Device(fields[0], Integer.parseInt(fields[1]), fields[2])); }
            catch (NumberFormatException e) { throw new IOException("Estado do driver DFU inválido", e); }
        }
        return List.copyOf(result);
    }
    @FunctionalInterface
    public interface Executor {
        CommandRunner.Result run(List<String> args, Path directory, Duration timeout, Consumer<String> log) throws IOException;
    }
    public static List<UsbPort> listPorts(Path tool) throws IOException {
        return listPorts(tool, CommandRunner::run);
    }
    private static List<UsbPort> listPorts(Path tool, Executor executor) throws IOException {
        CommandRunner.Result r = executor.run(List.of(tool.toString(), "-l", "usb"), tool.getParent(),
            Duration.ofSeconds(20), ignored -> {});
        if (r.exitCode() != 0) throw new IOException("Não foi possível enumerar o USB na ferramenta ST: " + r.output());
        return parsePorts(r.output());
    }
    public static List<UsbPort> parsePorts(String output) throws IOException {
        List<UsbPort> ports = new ArrayList<>();
        String current = null;
        Pattern portLine = Pattern.compile("(?i)^\\s*(?:USB\\s+Port|Device\\s+Index)\\s*:\\s*(USB[0-9]+)\\s*$");
        Pattern serialLine = Pattern.compile("(?i)^\\s*Serial\\s+number\\s*:\\s*([A-Za-z0-9]+)\\s*$");
        for (String line : output.split("\\R")) {
            Matcher p = portLine.matcher(line), s = serialLine.matcher(line);
            if (p.matches()) current = p.group(1).toUpperCase(Locale.ROOT);
            else if (s.matches() && current != null) {
                UsbPort device = new UsbPort(current, s.group(1));
                if (ports.stream().anyMatch(x -> x.serial().equalsIgnoreCase(device.serial()) || x.port().equals(device.port())))
                    throw new IOException("Identificação USB duplicada na ferramenta ST");
                ports.add(device); current = null;
            }
        }
        return List.copyOf(ports);
    }
    public static void flash(Path tool, UsbPort selected, FirmwareImage image, Consumer<String> log) throws IOException {
        flash(tool, selected, image, log, CommandRunner::run);
    }
    public static void flash(Path tool, UsbPort selected, FirmwareImage image, Consumer<String> log, Executor executor) throws IOException {
        if (!image.mode().equals("dfu")) throw new IOException("Imagem incompatível com DFU");
        // Re-enumerate and bind by serial number: USB1 can change after unplug/replug.
        UsbPort live = listPorts(tool, executor).stream().filter(p -> p.serial().equalsIgnoreCase(selected.serial())).findFirst()
            .orElseThrow(() -> new IOException("O dispositivo DFU selecionado foi desligado"));
        image.verifyUnchanged();
        CommandRunner.Result info = executor.run(List.of(tool.toString(), "-c", "port=" + live.port(),
            "sn=" + live.serial()), tool.getParent(), Duration.ofSeconds(20), log);
        // CubeProgrammer versions print the same numeric ID as 0x419 or 0x0419.
        if (info.exitCode() != 0 || !Pattern.compile("(?im)^\\h*Device\\h+ID\\h*:\\h*0x0*419\\h*$").matcher(info.output()).find())
            throw new IOException("O dispositivo não confirmou o grupo STM32F42x/F43x esperado. Nenhuma gravação iniciada.");
        // No mass erase, unprotect, option-byte changes, or start-at-base command.
        try (FirmwareImage.Staged staged = image.stage()) {
            CommandRunner.Result write = executor.run(writeCommand(tool, live, staged.path(), image.address()),
                tool.getParent(), Duration.ofMinutes(5), log);
            if (write.exitCode() != 0 || !write.output().contains("Download verified successfully"))
                throw new IOException("A ferramenta ST não confirmou a verificação da gravação. Consulta o registo.");
        }
        log.accept("\nGravação verificada. Coloca BOOT0 em arranque normal e reinicia a ECU.\n");
    }
    public static List<String> writeCommand(Path tool, UsbPort port, Path binary, long address) {
        return List.of(tool.toString(), "-c", "port=" + port.port(), "sn=" + port.serial(),
            "-w", binary.toString(), "0x" + Long.toHexString(address), "-v");
    }
}
