package org.tunuino.ui.protocol;

/** Decoder for the native Tunuino Console telemetry frame. */
public final class TelemetryFrame {
    private static final int PAYLOAD_SIZE = 14;
    private static final int FRAME_SIZE = 8 + PAYLOAD_SIZE;
    private TelemetryFrame() { }

    public static final class Reading {
        public final int rpm;
        public final double mapKpa;
        public final double coolantC;
        public final double batteryV;
        public Reading(int rpm, double mapKpa, double coolantC, double batteryV) {
            this.rpm = rpm;
            this.mapKpa = mapKpa;
            this.coolantC = coolantC;
            this.batteryV = batteryV;
        }
    }

    private static int u16(byte[] data, int offset) {
        return (data[offset] & 255) | ((data[offset + 1] & 255) << 8);
    }

    private static int crc16(byte[] data, int start, int count) {
        int crc = 0xffff;
        for (int i = start; i < start + count; i++) {
            crc ^= (data[i] & 255) << 8;
            for (int bit = 0; bit < 8; bit++)
                crc = ((crc & 0x8000) != 0 ? (crc << 1) ^ 0x1021 : crc << 1) & 0xffff;
        }
        return crc;
    }

    public static Reading decode(byte[] frame) {
        if (frame == null || frame.length != FRAME_SIZE) throw new IllegalArgumentException("Invalid telemetry length");
        if ((frame[0] & 255) != 0x54 || (frame[1] & 255) != 0x4e ||
            (frame[2] & 255) != 1 || (frame[3] & 255) != 2 || u16(frame, 4) != PAYLOAD_SIZE)
            throw new IllegalArgumentException("Invalid telemetry header");
        if (crc16(frame, 2, FRAME_SIZE - 4) != u16(frame, FRAME_SIZE - 2))
            throw new IllegalArgumentException("Telemetry checksum mismatch");
        return new Reading(u16(frame, 6), u16(frame, 8) / 10.0,
            u16(frame, 12) / 10.0 - 40.0, u16(frame, 18) / 100.0);
    }
}
