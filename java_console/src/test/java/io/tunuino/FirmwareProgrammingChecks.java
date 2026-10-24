/* Copyright (c) 2026 Tunuino. */
package io.tunuino;

import io.tunuino.maintenance.*;
import java.io.*;
import java.nio.*;
import java.nio.file.*;
import java.time.Duration;
import java.util.*;
import java.util.concurrent.atomic.AtomicInteger;

public final class FirmwareProgrammingChecks {
    private static void check(boolean value, String message) { if (!value) throw new AssertionError(message); }
    private interface Operation { void run() throws Exception; }
    private static void reject(Operation operation) throws Exception {
        try { operation.run(); } catch (IOException expected) { return; }
        throw new AssertionError("Unsafe input accepted");
    }
    private static Path image(Path root) throws IOException {
        Path bin = root.resolve("firmware with spaces.bin");
        byte[] bytes = new byte[64];
        ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN).putInt(0x20010000).putInt(0x08000021);
        Files.write(bin, bytes);
        manifest(bin, "proteus-v07-f427", "dfu", "0x08000000", true);
        return bin;
    }
    private static void manifest(Path file, String target, String mode, String base, boolean replaces) throws IOException {
        Files.writeString(file.resolveSibling(file.getFileName() + ".properties"), "format=1\ntarget=" + target
            + "\nmode=" + mode + "\naddress=" + base + "\nreplaces_bootloader=" + replaces
            + "\nsha256=" + FirmwareImage.digest(Files.readAllBytes(file)) + "\n");
    }
    private static String record(long addr, byte[] data) {
        byte[] bytes = new byte[6 + data.length];
        bytes[0] = (byte)(5 + data.length);
        ByteBuffer.wrap(bytes, 1, 4).putInt((int)addr);
        System.arraycopy(data, 0, bytes, 5, data.length);
        int sum = 0; for (byte b : bytes) sum += b & 255;
        bytes[bytes.length - 1] = (byte)~sum;
        return "S3" + HexFormat.of().formatHex(bytes);
    }
    private static CommandRunner.Result result(String output) { return new CommandRunner.Result(0, output); }
    public static void main(String[] args) throws Exception {
        if (args.length > 0 && !args[0].matches("[0-9a-f]{40}")) {
            if (args[0].equals("wait")) Thread.sleep(30000);
            if (args[0].equals("flood")) System.out.print("X".repeat(400000));
            else if (args[0].equals("echo")) System.out.println(args[1]);
            return;
        }
        if (args.length > 0) check(BuildInfo.revision().equals(args[0]), "Release JAR must identify its actual source commit");
        Path root = Files.createTempDirectory("tunuino firmware checks ");
        try {
            check(WindowsDfu.parseDevices("").isEmpty(), "No device must remain empty");
            var devices = WindowsDfu.parseDevices("USB\\VID_0483&PID_DF11\\ABC|0|STM32 BOOTLOADER\nUSB\\VID_0483&PID_DF11\\DEF|28|Dispositivo desconhecido");
            check(devices.size() == 2 && devices.get(1).driverError() == 28, "Driver errors must remain visible");
            reject(() -> WindowsDfu.parseDevices("COM3|0|not DFU"));
            String ports = "Device Index : USB1\nSerial number : ABC123\nUSB Port : USB2\nSerial number : DEF456\n";
            check(WindowsDfu.parsePorts(ports).size() == 2, "Enumerate independent devices");
            reject(() -> WindowsDfu.parsePorts(ports + "USB Port : USB3\nSerial number : ABC123\n"));
            Path bin = image(root);
            FirmwareImage fw = FirmwareImage.load(bin);
            try (var staged = fw.stage()) { check(Arrays.equals(Files.readAllBytes(bin), Files.readAllBytes(staged.path())), "Stage must preserve exact bytes"); }
            Path tool = root.resolve("STM32 tool/STM32_Programmer_CLI.exe");
            var usb = new WindowsDfu.UsbPort("USB1", "ABC123");
            var command = WindowsDfu.writeCommand(tool, usb, bin, 0x08000000L);
            check(command.contains(bin.toString()) && command.contains("-v") && !command.contains("-s") && !command.contains("-e"), "No implicit start/erase; verify mandatory");
            AtomicInteger writes = new AtomicInteger();
            WindowsDfu.Executor good = (cmd, cwd, timeout, log) -> {
                if (cmd.contains("-l")) return result("Device Index : USB2\nSerial number : ABC123\n");
                check(cmd.contains("port=USB2") && cmd.contains("sn=ABC123"), "Rebind by serial, never stale USB index");
                if (cmd.contains("-w")) { writes.incrementAndGet(); return result("Download verified successfully"); }
                return result("Device ID : 0x419");
            };
            WindowsDfu.flash(tool, usb, fw, ignored -> {}, good);
            check(writes.get() == 1, "Exactly one write expected");
            for (String id : List.of("0x0419", "0X000419", "\u001b[32m0x0419\u001b[0m", "0x0419\u0000")) {
                AtomicInteger paddedWrites = new AtomicInteger();
                WindowsDfu.flash(tool, usb, fw, ignored -> {}, (cmd, cwd, timeout, log) -> {
                    if (cmd.contains("-l")) return result("Device Index : USB1\r\nSerial number : ABC123\r\n");
                    if (cmd.contains("-w")) {
                        paddedWrites.incrementAndGet();
                        return result("Download verified successfully");
                    }
                    // CubeProgrammer v2.19.0 output reported on a physical Proteus v0.7.
                    return result("STM32CubeProgrammer v2.19.0\r\n"
                        + "Product ID : STM32 BOOTLOADER\r\n"
                        + "Device ID : " + id + "\r\nDevice name : STM32F42xxx/F43xxx\r\n"
                        + "Flash size : 2 MBytes (default)\r\nDevice CPU : Cortex-M4\r\n");
                });
                check(paddedWrites.get() == 1, "Padded F427 device ID must allow one verified write: " + id);
            }
            check(WindowsDfu.parseDeviceId("Device ID : 0x0419\nDevice ID : 0x419") == 0x419, "Equivalent repeated numeric IDs");
            reject(() -> WindowsDfu.parseDeviceId("Device ID : 0x0419\nDevice ID : 0x450"));
            reject(() -> WindowsDfu.parseDeviceId("Device ID : 0x0419xxxx"));
            for (String id : List.of("0x0450", "0x04190", "0x4190", "", "unreadable")) {
                reject(() -> WindowsDfu.flash(tool, usb, fw, ignored -> {}, (cmd, cwd, timeout, log) -> {
                    if (cmd.contains("-l")) return result("Device Index : USB1\nSerial number : ABC123\n");
                    check(!cmd.contains("-w"), "Unknown or different device ID must stop before writing");
                    return result("Device ID : " + id);
                }));
            }
            try {
                WindowsDfu.flash(tool, usb, fw, ignored -> {}, (cmd, cwd, timeout, log) -> {
                if (cmd.contains("-l")) return result("Device Index : USB1\nSerial number : ABC123\n");
                check(!cmd.contains("-w"), "Failed connection must stop before writing even with matching ID");
                return new CommandRunner.Result(1, "Device ID : 0x0419");
                });
                throw new AssertionError("Nonzero ST status accepted");
            } catch (IOException e) {
                check(e.getMessage().contains("código 1"), "Connection failure must show exit status instead of blaming the MCU ID");
            }
            reject(() -> WindowsDfu.flash(tool, usb, fw, ignored -> {}, (cmd, cwd, timeout, log) -> {
                if (cmd.contains("-l")) return result("Device Index : USB1\nSerial number : ABC123\n");
                check(!cmd.contains("-w"), "Wrong MCU must stop before writing");
                return result("Device ID : 0x450");
            }));
            reject(() -> WindowsDfu.flash(tool, usb, fw, ignored -> {}, (cmd, cwd, timeout, log) -> {
                if (cmd.contains("-l")) return result("Device Index : USB1\nSerial number : OTHER\n");
                throw new AssertionError("Disconnected target must not be opened");
            }));
            reject(() -> WindowsDfu.flash(tool, usb, fw, ignored -> {}, (cmd, cwd, timeout, log) -> {
                if (cmd.contains("-w")) return result("Download complete without verification");
                return good.run(cmd, cwd, timeout, log);
            }));
            reject(() -> WindowsDfu.flash(tool, usb, fw, ignored -> {}, (cmd, cwd, timeout, log) -> {
                if (cmd.contains("-w")) return new CommandRunner.Result(1, "Download verified successfully");
                return good.run(cmd, cwd, timeout, log);
            }));
            manifest(bin, "uaefi-f767", "dfu", "0x08000000", true);
            reject(() -> FirmwareImage.load(bin));
            manifest(bin, "proteus-v07-f427", "dfu", "0x08008000", true);
            reject(() -> FirmwareImage.load(bin));
            manifest(bin, "proteus-v07-f427", "dfu", "0x08000000", false);
            reject(() -> FirmwareImage.load(bin));
            Files.write(bin, new byte[64]);
            reject(fw::verifyUnchanged);
            reject(fw::stage);
            manifest(bin, "proteus-v07-f427", "dfu", "0x08000000", true);
            reject(() -> FirmwareImage.load(bin));
            byte[] data = new byte[64];
            ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN).putInt(0x20010000).putInt(0x08008021);
            String srec = record(0x08008000L, data);
            FirmwareImage.validateSrec(srec, 0x08008000L);
            reject(() -> FirmwareImage.validateSrec(srec.substring(0, srec.length() - 2) + "00", 0x08008000L));
            reject(() -> FirmwareImage.validateSrec(srec + "\n" + srec, 0x08008000L));
            reject(() -> FirmwareImage.validateSrec(record(0x08000000, data), 0x08008000L));
            reject(() -> FirmwareImage.validateSrec(record(0x08100000L, data), 0x08008000L));
            Path update = root.resolve("firmware.srec"); Files.writeString(update, srec);
            manifest(update, "proteus-v07-f427", "openblt", "0x08008000", false);
            check(FirmwareImage.load(update).mode().equals("openblt"), "Valid OpenBLT image");
            check(OpenBltProgrammer.command(tool, "COM3", update).contains("-t=xcp_rs232"), "Explicit OpenBLT transport");
            reject(() -> OpenBltProgrammer.command(tool, "COM3 & echo BAD", update));
            reject(() -> OpenBltProgrammer.flash(tool, "COM3", fw, ignored -> {}));
            String javaExe = Path.of(System.getProperty("java.home"), "bin", System.getProperty("os.name").startsWith("Windows") ? "java.exe" : "java").toString();
            List<String> child = List.of(javaExe, "-cp", System.getProperty("java.class.path"), FirmwareProgrammingChecks.class.getName());
            List<String> echo = new ArrayList<>(child); echo.addAll(List.of("echo", "path with spaces & $() literal"));
            check(CommandRunner.run(echo, null, Duration.ofSeconds(10), ignored -> {}).output().contains("path with spaces & $() literal"), "No shell interpolation");
            List<String> flood = new ArrayList<>(child); flood.add("flood");
            check(CommandRunner.run(flood, null, Duration.ofSeconds(10), ignored -> {}).output().length() <= 262144, "Bounded output");
            List<String> wait = new ArrayList<>(child); wait.add("wait");
            long start = System.nanoTime();
            reject(() -> CommandRunner.run(wait, null, Duration.ofMillis(250), ignored -> {}));
            check(System.nanoTime() - start < Duration.ofSeconds(8).toNanos(), "Timed-out process must be killed");
            System.out.println("Firmware checks passed: target/hash/vectors, SREC bounds/overlap, USB serial rebinding, wrong MCU, driver state, failed verification, process timeout and output limits. Simulated tools; no physical ECU.");
        } finally {
            try (var paths = Files.walk(root)) { for (Path p : paths.sorted(Comparator.reverseOrder()).toList()) Files.delete(p); }
        }
    }
}
