/* Copyright (c) 2026 Tunuino. */
package io.tunuino.protocol;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.HexFormat;
import java.util.Locale;
import java.util.Objects;

/** Self-reported firmware identity; not independent proof of a board pin map. */
public record DeviceIdentity(Family family, McuSeries mcuSeries, int hardwareRevision,
        int firmwareMajor, int firmwareMinor, int firmwarePatch, long capabilities, String uid) {
    public enum Family {
        PROTEUS(1, "Proteus"), UAEFI(2, "uaEFI");
        private final int id;
        private final String label;
        Family(int id, String label) { this.id = id; this.label = label; }
        public int id() { return id; }
        @Override public String toString() { return label; }
    }

    public enum McuSeries { STM32_F4, STM32_F7, STM32_H7 }

    public DeviceIdentity {
        Objects.requireNonNull(family, "family");
        Objects.requireNonNull(mcuSeries, "mcuSeries");
        Objects.requireNonNull(uid, "uid");
        if (hardwareRevision < 1 || hardwareRevision > 65535
                || firmwareMajor < 0 || firmwareMajor > 255
                || firmwareMinor < 0 || firmwareMinor > 255
                || firmwarePatch < 0 || firmwarePatch > 255
                || capabilities < 0 || (capabilities & ~0x1FL) != 0
                || !uid.matches("[0-9a-fA-F]{24}")) {
            throw new IllegalArgumentException("Identidade inválida");
        }
        uid = uid.toLowerCase(Locale.ROOT);
        if (uid.equals("0".repeat(24)) || uid.equals("f".repeat(24))) {
            throw new IllegalArgumentException("UID não configurado");
        }
    }

    public String firmwareVersion() {
        return firmwareMajor + "." + firmwareMinor + "." + firmwarePatch;
    }

    public static DeviceIdentity decode(byte[] body) throws IOException {
        if (body == null || body.length != 32) {
            throw new IOException("Resposta de identificação deve ter 32 bytes");
        }
        if (body[0] != 0 || !new String(body, 1, 7, StandardCharsets.US_ASCII).equals("TUNUINO")) {
            throw new IOException("A resposta não identifica firmware Tunuino");
        }
        if (body[8] != 1) {
            throw new IOException("Versão do protocolo de identificação não suportada");
        }
        Family family = switch (body[9] & 255) {
            case 1 -> Family.PROTEUS;
            case 2 -> Family.UAEFI;
            default -> throw new IOException("Família de centralina desconhecida");
        };
        McuSeries mcu = switch (body[10] & 255) {
            case 1 -> McuSeries.STM32_F4;
            case 2 -> McuSeries.STM32_F7;
            case 3 -> McuSeries.STM32_H7;
            default -> throw new IOException("Série STM32 desconhecida");
        };
        ByteBuffer bytes = ByteBuffer.wrap(body);
        try {
            return new DeviceIdentity(family, mcu, Short.toUnsignedInt(bytes.getShort(11)),
                body[13] & 255, body[14] & 255, body[15] & 255,
                Integer.toUnsignedLong(bytes.getInt(16)), HexFormat.of().formatHex(body, 20, 32));
        } catch (IllegalArgumentException e) {
            throw new IOException(e.getMessage(), e);
        }
    }
}
