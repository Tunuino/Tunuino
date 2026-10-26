/* Copyright (c) 2026 Tunuino. */
package io.tunuino.protocol;

import io.tunuino.transport.PacketSession;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.ArrayList;
import java.util.List;

/** Read-only preview of existing ECU maps, not engine output or safety authorization. */
public final class EnginePreviewClient {
    public static final int NO_RPM = 0x01;
    public static final int NO_MAP = 0x02;
    public static final int TRIGGER_DISABLED = 0x04;
    public static final int NO_SYNC = 0x08;
    public static final int BAD_ADC = 0x10;
    public static final int RPM_LIMIT = 0x20;
    public static final int OUT_OF_TABLE = 0x40;
    public static final int UNCALIBRATED = 0x80;
    private static final byte[] REQUEST = "TUNUINO:E".getBytes(StandardCharsets.US_ASCII);
    private EnginePreviewClient() {}

    public record Preview(boolean valid, int reasons, double vePercent, double advanceDegrees,
                          double rpm, double mapKpa, long generation) {
        public String explanation() {
            if (valid) return "Cálculo de bancada disponível; saídas do motor desativadas";
            return explainReasons(reasons);
        }
    }
    public static String explainReasons(int reasons) {
            List<String> causes = new ArrayList<>();
            if ((reasons & NO_RPM) != 0) causes.add("RPM indisponíveis");
            if ((reasons & NO_MAP) != 0) causes.add("MAP indisponível");
            if ((reasons & TRIGGER_DISABLED) != 0) causes.add("captura desativada");
            if ((reasons & NO_SYNC) != 0) causes.add("cambota sem sincronização");
            if ((reasons & BAD_ADC) != 0) causes.add("ADC inválido");
            if ((reasons & RPM_LIMIT) != 0) causes.add("limite de RPM");
            if ((reasons & OUT_OF_TABLE) != 0) causes.add("fora dos eixos dos mapas");
            if ((reasons & UNCALIBRATED) != 0) causes.add("mapas não calibrados");
            return String.join(" · ", causes);
    }

    public static boolean supports(DeviceIdentity identity) {
        return identity != null && identity.family() == DeviceIdentity.Family.PROTEUS
            && identity.mcuSeries() == DeviceIdentity.McuSeries.STM32_F4
            && identity.hardwareRevision() == 7 && identity.firmwareVersion().equals("0.2.0")
            && identity.capabilities() == 15;
    }

    /** Decode the exact 20-byte status-prefixed format v1; reject silent garbage. */
    public static Preview decode(byte[] body) throws IOException {
        if (body == null || body.length != 20 || body[0] != 0 || body[1] != 1
                || (body[2] != 0 && body[2] != 1)) {
            throw new IOException("Resposta de pré-visualização Tunuino incompatível");
        }
        ByteBuffer b = ByteBuffer.wrap(body); // Big-endian wire values.
        boolean valid = body[2] == 1;
        int reasons = body[3] & 255, ve = Short.toUnsignedInt(b.getShort(4));
        int ignition = b.getShort(6);
        long rpm = Integer.toUnsignedLong(b.getInt(8));
        long map = Integer.toUnsignedLong(b.getInt(12));
        long generation = Integer.toUnsignedLong(b.getInt(16));
        if (generation == 0 || ve > 30000 || ignition < -9000 || ignition > 9000
                || rpm > 20_000_000L || map > 500_000L
                || (valid && (reasons != 0 || ve == 0 || rpm == 0 || map == 0))
                || (!valid && (reasons == 0 || ve != 0 || ignition != 0))) {
            throw new IOException("Valores ou estado da pré-visualização incoerentes");
        }
        return new Preview(valid, reasons, ve / 100.0, ignition / 100.0,
            rpm / 1000.0, map / 1000.0, generation);
    }

    public static Preview read(PacketSession session, DeviceIdentity identity, Duration timeout)
            throws IOException {
        if (!supports(identity))
            throw new IllegalStateException("Este firmware não suporta a pré-visualização F427 IO");
        try {
            return decode(session.exchange(REQUEST, timeout));
        } catch (IOException error) {
            try { session.close(); }
            catch (IOException closing) { error.addSuppressed(closing); }
            throw error;
        }
    }
}
