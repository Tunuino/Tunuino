package org.tunuino.ui.protocol;

public final class TelemetryFrameSmokeTest {
    private static byte[] fromHex(String hex) {
        byte[] bytes = new byte[hex.length() / 2];
        for (int i = 0; i < bytes.length; i++) bytes[i] = (byte) Integer.parseInt(hex.substring(i * 2, i * 2 + 2), 16);
        return bytes;
    }
    public static void main(String[] args) {
        byte[] packet = fromHex("544e01020e00dc05f5034501e2048a02be0564059dad");
        TelemetryFrame.Reading reading = TelemetryFrame.decode(packet);
        if (reading.rpm != 1500 || reading.mapKpa != 101.3 || reading.coolantC != 85.0 || reading.batteryV != 13.8)
            throw new AssertionError("Telemetry fields incorrect");
        packet[6] ^= 1;
        try {
            TelemetryFrame.decode(packet);
            throw new AssertionError("Corrupted frame accepted");
        } catch (IllegalArgumentException expected) { }
    }
}
