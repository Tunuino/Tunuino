/* Copyright (c) 2026 Tunuino. */
package io.tunuino.maintenance;

import java.io.*;
import java.nio.*;
import java.nio.file.*;
import java.security.*;
import java.util.*;

/** Explicit target manifest, checksum, vectors and address bounds before any programmer is opened. */
public record FirmwareImage(Path file, String mode, long address, String sha256, String description,
                            boolean replacesBootloader) {
    public static FirmwareImage load(Path selected) throws IOException {
        Path file = selected.toAbsolutePath().normalize();
        if (Files.size(file) > 8 * 1024 * 1024) throw new IOException("Ficheiro demasiado grande");
        Properties p = new Properties();
        Path manifest = file.resolveSibling(file.getFileName() + ".properties");
        if (Files.size(manifest) > 16384) throw new IOException("Manifesto demasiado grande");
        try (Reader reader = Files.newBufferedReader(manifest)) { p.load(reader); }
        if (!"1".equals(p.getProperty("format")) || !"proteus-v07-f427".equals(p.getProperty("target")))
            throw new IOException("É obrigatório um manifesto para Proteus v0.7 STM32F427");
        String mode = p.getProperty("mode", "");
        long address;
        try { address = Long.decode(p.getProperty("address", "")); }
        catch (NumberFormatException e) { throw new IOException("Endereço inválido", e); }
        String hash = p.getProperty("sha256", "").toLowerCase(Locale.ROOT);
        if (!hash.matches("[a-f0-9]{64}")) throw new IOException("SHA-256 em falta ou inválido");
        byte[] bytes = Files.readAllBytes(file);
        if (!hash.equals(digest(bytes))) throw new IOException("SHA-256 não corresponde ao firmware");
        if ("dfu".equals(mode)) {
            if (!file.toString().endsWith(".bin") || address != 0x08000000L || bytes.length < 8 || bytes.length > 128 * 1024)
                throw new IOException("Imagem DFU de bancada inválida");
            if (!"true".equals(p.getProperty("replaces_bootloader")))
                throw new IOException("Falta declarar a substituição do bootloader");
            validateVectors(bytes, address, address + bytes.length);
        } else if ("openblt".equals(mode)) {
            if (!file.toString().endsWith(".srec") || address < 0x08008000L || address >= 0x08100000L
                    || !"false".equals(p.getProperty("replaces_bootloader")))
                throw new IOException("Imagem OpenBLT inválida ou sobrepõe o bootloader");
            validateSrec(new String(bytes, java.nio.charset.StandardCharsets.US_ASCII), address);
        } else throw new IOException("Modo de gravação desconhecido");
        String description = p.getProperty("description", "Proteus v0.7 STM32F427");
        return new FirmwareImage(file, mode, address, hash, description, mode.equals("dfu"));
    }
    private static void validateVectors(byte[] bytes, long base, long end) throws IOException {
        ByteBuffer b = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);
        long stack = Integer.toUnsignedLong(b.getInt()), reset = Integer.toUnsignedLong(b.getInt());
        if (stack <= 0x20000000L || stack > 0x20030000L || (stack & 7) != 0
                || (reset & 1) == 0 || (reset & ~1L) < base || (reset & ~1L) >= end)
            throw new IOException("Vetores de arranque inválidos para STM32F427");
    }
    public static void validateSrec(String text, long base) throws IOException {
        TreeMap<Long, byte[]> segments = new TreeMap<>();
        for (String line : text.split("\\R")) {
            if (line.isBlank()) continue;
            line = line.strip();
            if (!line.matches("S[012356789][0-9A-Fa-f]+") || (line.length() & 1) != 0 || line.length() > 514)
                throw new IOException("S-record inválido");
            char kind = line.charAt(1);
            int n = switch (kind) { case '0','1','5','9' -> 2; case '2','6','8' -> 3; default -> 4; };
            byte[] data;
            try { data = HexFormat.of().parseHex(line.substring(2)); }
            catch (IllegalArgumentException e) { throw new IOException("S-record hexadecimal inválido", e); }
            if (data.length < n + 2 || (data[0] & 255) != data.length - 1) throw new IOException("Comprimento S-record inválido");
            int sum = 0; for (byte b : data) sum += b & 255;
            if ((sum & 255) != 255) throw new IOException("Checksum S-record inválido");
            if (kind != '1' && kind != '2' && kind != '3') continue;
            long addr = 0; for (int i = 1; i <= n; i++) addr = (addr << 8) | (data[i] & 255);
            byte[] body = Arrays.copyOfRange(data, n + 1, data.length - 1);
            if (body.length == 0 || addr < base || addr + body.length > 0x08100000L)
                throw new IOException("S-record fora da área da aplicação F427");
            Map.Entry<Long, byte[]> previous = segments.floorEntry(addr), next = segments.ceilingEntry(addr);
            if ((previous != null && previous.getKey() + previous.getValue().length > addr)
                    || (next != null && addr + body.length > next.getKey()))
                throw new IOException("S-records sobrepostos");
            segments.put(addr, body);
        }
        if (segments.isEmpty() || segments.firstKey() != base) throw new IOException("Endereço inicial S-record incorreto");
        byte[] vectors = new byte[8];
        for (int i = 0; i < vectors.length; i++) {
            Map.Entry<Long, byte[]> segment = segments.floorEntry(base + i);
            if (segment == null || base + i >= segment.getKey() + segment.getValue().length)
                throw new IOException("Vetores S-record incompletos");
            vectors[i] = segment.getValue()[(int)(base + i - segment.getKey())];
        }
        validateVectors(vectors, base, segments.lastKey() + segments.lastEntry().getValue().length);
    }
    public void verifyUnchanged() throws IOException {
        if (!sha256.equals(digest(Files.readAllBytes(file)))) throw new IOException("O firmware foi alterado; seleciona-o novamente");
    }
    public Staged stage() throws IOException {
        byte[] data = Files.readAllBytes(file);
        if (!sha256.equals(digest(data))) throw new IOException("O firmware foi alterado antes da gravação");
        Path directory = Files.createTempDirectory("tunuino-firmware-");
        Path path = directory.resolve(mode.equals("dfu") ? "firmware.bin" : "firmware.srec");
        try { Files.write(path, data); return new Staged(path); }
        catch (IOException e) { Files.deleteIfExists(path); Files.deleteIfExists(directory); throw e; }
    }
    public record Staged(Path path) implements AutoCloseable {
        @Override public void close() throws IOException { Files.deleteIfExists(path); Files.deleteIfExists(path.getParent()); }
    }
    public static String digest(byte[] bytes) {
        try { return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes)); }
        catch (NoSuchAlgorithmException e) { throw new IllegalStateException(e); }
    }
}
