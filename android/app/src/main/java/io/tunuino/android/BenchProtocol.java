/* Copyright (c) 2026 Tunuino. */
package io.tunuino.android;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.Locale;
import java.util.zip.CRC32;

/** Read-only identity client, independent of Android so the C wire format can be tested. */
public final class BenchProtocol {
    public static final int MAX_BODY = 4096;
    private BenchProtocol() {}

    public interface Transport {
        int read(byte[] bytes, int offset, int count, int timeoutMs) throws IOException;
        int write(byte[] bytes, int offset, int count, int timeoutMs) throws IOException;
    }

    public static byte[] encode(byte[] body) {
        if (body == null || body.length < 1 || body.length > MAX_BODY) {
            throw new IllegalArgumentException("Comprimento do pedido inválido");
        }
        byte[] packet = new byte[body.length + 6];
        ByteBuffer.wrap(packet).putShort((short) body.length).put(body);
        CRC32 crc = new CRC32();
        crc.update(body);
        ByteBuffer.wrap(packet).putInt(packet.length - 4, (int) crc.getValue());
        return packet;
    }

    public static byte[] decode(byte[] packet) throws IOException {
        if (packet == null || packet.length < 7) throw new IOException("Resposta incompleta");
        int length = ((packet[0] & 255) << 8) | (packet[1] & 255);
        if (length < 1 || length > MAX_BODY || packet.length != length + 6) {
            throw new IOException("Comprimento da resposta inválido");
        }
        CRC32 crc = new CRC32();
        crc.update(packet, 2, length);
        if (crc.getValue() != (ByteBuffer.wrap(packet).getInt(length + 2) & 0xffffffffL)) {
            throw new IOException("CRC da resposta inválido");
        }
        return Arrays.copyOfRange(packet, 2, length + 2);
    }

    public static Identity identify(Transport transport) throws IOException {
        long deadline = System.nanoTime() + 2_000_000_000L;
        byte[] request = encode("TUNUINO?".getBytes(StandardCharsets.US_ASCII));
        transfer(transport, request, 0, request.length, deadline, true);
        byte[] header = new byte[2];
        transfer(transport, header, 0, 2, deadline, false);
        int count = ((header[0] & 255) << 8) | (header[1] & 255);
        if (count < 1 || count > MAX_BODY) throw new IOException("Comprimento da resposta inválido");
        byte[] packet = new byte[count + 6];
        System.arraycopy(header, 0, packet, 0, 2);
        transfer(transport, packet, 2, count + 4, deadline, false);
        return Identity.decode(decode(packet));
    }

    private static void transfer(Transport transport, byte[] bytes, int offset, int count,
            long deadline, boolean writing) throws IOException {
        int done = 0;
        while (done < count) {
            long remaining = deadline - System.nanoTime();
            if (remaining <= 0 || Thread.currentThread().isInterrupted()) {
                throw new IOException("Pedido USB expirado ou cancelado");
            }
            int timeout = (int) Math.max(1, (remaining + 999_999L) / 1_000_000L);
            int size = writing ? transport.write(bytes, offset + done, count - done, timeout)
                : transport.read(bytes, offset + done, count - done, timeout);
            if (size <= 0 || size > count - done) throw new IOException("USB sem resposta ou desligado");
            done += size;
        }
    }

    public static final class Identity {
        public final String family, mcu, version, uid;
        public final int revision;
        public final long capabilities;

        private Identity(String family, String mcu, int revision, String version, String uid, long capabilities) {
            this.family = family; this.mcu = mcu; this.revision = revision;
            this.version = version; this.uid = uid; this.capabilities = capabilities;
        }

        public static Identity decode(byte[] body) throws IOException {
            if (body == null || body.length != 32 || body[0] != 0 || body[8] != 1
                    || !"TUNUINO".equals(new String(body, 1, 7, StandardCharsets.US_ASCII))) {
                throw new IOException("O dispositivo não respondeu com uma identidade Tunuino válida");
            }
            String[] families = {"", "Proteus", "uaEFI"};
            String[] mcus = {"", "STM32 F4", "STM32 F7", "STM32 H7"};
            int family = body[9] & 255, mcu = body[10] & 255;
            int revision = ((body[11] & 255) << 8) | (body[12] & 255);
            long caps = ByteBuffer.wrap(body).getInt(16) & 0xffffffffL;
            if (family < 1 || family >= families.length || mcu < 1 || mcu >= mcus.length
                    || revision == 0 || (caps & ~31L) != 0) throw new IOException("Identidade não suportada");
            StringBuilder uid = new StringBuilder();
            for (int i = 20; i < 32; i++) uid.append(String.format(Locale.ROOT, "%02x", body[i] & 255));
            if (uid.toString().equals("000000000000000000000000")
                    || uid.toString().equals("ffffffffffffffffffffffff")) throw new IOException("UID inválido");
            return new Identity(families[family], mcus[mcu], revision,
                (body[13] & 255) + "." + (body[14] & 255) + "." + (body[15] & 255), uid.toString(), caps);
        }
    }
}
