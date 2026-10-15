package org.tunuino.ui.protocol;

import java.io.ByteArrayInputStream;
import java.util.ArrayList;
import java.util.List;

public final class TelemetryStreamReaderSmokeTest {
    private static byte[] hex(String s) {
        byte[] b = new byte[s.length() / 2];
        for (int i = 0; i < b.length; i++) b[i] = (byte) Integer.parseInt(s.substring(i * 2, i * 2 + 2), 16);
        return b;
    }
    public static void main(String[] args) throws Exception {
        byte[] valid = hex("544e01020e00dc05f5034501e2048a02be0564059dad");
        byte[] invalid = valid.clone();
        invalid[6] ^= 1;
        List<byte[]> received = new ArrayList<>();
        TelemetryStreamReader reader = new TelemetryStreamReader(received::add);
        reader.accept(0x00);
        reader.accept(0x54);
        reader.accept(0x00);
        for (byte b : invalid) reader.accept(b & 255);
        byte[] combined = new byte[valid.length * 2];
        System.arraycopy(valid, 0, combined, 0, valid.length);
        System.arraycopy(valid, 0, combined, valid.length, valid.length);
        reader.readFrom(new ByteArrayInputStream(combined));
        if (received.size() != 2) throw new AssertionError("Expected two valid frames, got " + received.size());
        if (TelemetryFrame.decode(received.get(0)).rpm != 1500) throw new AssertionError("RPM mismatch");
    }
}
