package org.tunuino.ui.protocol;

import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;
import java.util.Objects;
import java.util.function.Consumer;

/** Reads a continuous byte stream and extracts validated Tunuino telemetry frames. */
public final class TelemetryStreamReader {
    private static final int HEADER_SIZE = 6;
    private static final int MAX_PAYLOAD = 64;
    private final byte[] pending = new byte[HEADER_SIZE + MAX_PAYLOAD + 2];
    private int count;
    private final Consumer<byte[]> receiver;

    public TelemetryStreamReader(Consumer<byte[]> receiver) {
        this.receiver = Objects.requireNonNull(receiver, "receiver");
    }

    public void accept(int nextByte) {
        if (nextByte < 0 || nextByte > 255) throw new IllegalArgumentException("byte out of range");
        if (count == 0 && nextByte != 0x54) return;
        pending[count++] = (byte) nextByte;
        if (count == 2 && (pending[1] & 255) != 0x4e) {
            discardOne();
            return;
        }
        if (count < HEADER_SIZE) return;
        int length = (pending[4] & 255) | ((pending[5] & 255) << 8);
        if ((pending[2] & 255) != 1 || length > MAX_PAYLOAD) {
            discardOne();
            return;
        }
        int total = HEADER_SIZE + length + 2;
        if (count == total) {
            byte[] packet = Arrays.copyOf(pending, total);
            discardAll();
            if ((packet[3] & 255) == 2 && length == 14) {
                try {
                    TelemetryFrame.decode(packet);
                    receiver.accept(packet);
                } catch (IllegalArgumentException ignored) {
                    // Invalid packets never reach the dashboard.
                }
            }
        }
    }

    private void discardAll() { count = 0; }

    private void discardOne() {
        int old = count;
        count = 0;
        for (int i = 1; i < old; i++) accept(pending[i] & 255);
    }

    /** Blocking read loop; the caller owns and closes the input stream. */
    public void readFrom(InputStream input) throws IOException {
        Objects.requireNonNull(input, "input");
        int value;
        while ((value = input.read()) != -1) accept(value);
    }
}
