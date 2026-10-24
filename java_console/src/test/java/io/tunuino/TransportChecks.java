/* Copyright (c) 2026 Tunuino. */
package io.tunuino;

import io.tunuino.protocol.FrameCodec;
import io.tunuino.transport.PacketSession;
import io.tunuino.transport.SerialChannel;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InterruptedIOException;
import java.net.SocketTimeoutException;
import java.time.Duration;
import java.util.Arrays;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

public final class TransportChecks {
    private static final Duration TIMEOUT = Duration.ofSeconds(2);

    private static void check(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError(message);
        }
    }

    private static final class Channel implements SerialChannel {
        final byte[] incoming;
        final ByteArrayOutputStream outgoing = new ByteArrayOutputStream();
        int position;
        int closes;
        int maxReadRequest;
        boolean disconnected;

        Channel(byte[] incoming) {
            this.incoming = incoming;
        }

        @Override public int read(byte[] buffer, int offset, int count, int timeoutMs) {
            check(timeoutMs > 0 && timeoutMs <= 2000, "Bounded read timeout");
            maxReadRequest = Math.max(count, maxReadRequest);
            if (disconnected) {
                return -1;
            }
            int n = Math.min(2, Math.min(count, incoming.length - position));
            System.arraycopy(incoming, position, buffer, offset, n);
            position += n;
            return n;
        }

        @Override public int write(byte[] buffer, int offset, int count, int timeoutMs) {
            check(timeoutMs > 0 && timeoutMs <= 2000, "Bounded write timeout");
            int n = Math.min(3, count);
            outgoing.write(buffer, offset, n);
            return n;
        }

        @Override public void close() {
            ++closes;
        }
    }

    private static void failedResponse(byte[] response) throws Exception {
        Channel channel = new Channel(response);
        PacketSession session = new PacketSession(channel);
        try {
            session.exchange(new byte[] {'Q'}, TIMEOUT);
            throw new AssertionError("Invalid response was accepted");
        } catch (IOException expected) {
            check(!session.isOpen() && channel.closes == 1, "Failure invalidates session");
        }
        int written = channel.outgoing.size();
        try {
            session.exchange(new byte[] {'Q'}, TIMEOUT);
            throw new AssertionError("Closed session was reused");
        } catch (IOException expected) {
            check(channel.outgoing.size() == written, "No retransmission after failure");
        }
        session.close();
        check(channel.closes == 1, "Close is idempotent");
    }

    public static void main(String[] args) throws Exception {
        byte[] request = new byte[] {'Q', 1, 2, 3};
        byte[] answer = new byte[] {0, 10, 20, 30, 40};
        byte[] frame = FrameCodec.encode(answer);
        Channel fragments = new Channel(frame);
        try (PacketSession session = new PacketSession(fragments)) {
            check(Arrays.equals(answer, session.exchange(request, TIMEOUT)), "Fragmented response");
            check(Arrays.equals(FrameCodec.encode(request), fragments.outgoing.toByteArray()), "Partial writes preserve request");
        }
        check(fragments.closes == 1, "Normal close");
        for (int length = 0; length < frame.length; ++length) {
            failedResponse(Arrays.copyOf(frame, length));
        }
        byte[] badCrc = frame.clone();
        badCrc[badCrc.length - 1] ^= 1;
        failedResponse(badCrc);
        failedResponse(new byte[] {0, 0});
        failedResponse(new byte[] {(byte) 0xff, (byte) 0xff});

        Channel disconnected = new Channel(frame);
        disconnected.disconnected = true;
        try (PacketSession session = new PacketSession(disconnected)) {
            try {
                session.exchange(request, TIMEOUT);
                throw new AssertionError("Disconnect was accepted");
            } catch (IOException expected) {
                check(!session.isOpen(), "Disconnect invalidates session");
            }
        }

        Channel interrupted = new Channel(frame);
        try (PacketSession session = new PacketSession(interrupted)) {
            Thread.currentThread().interrupt();
            try {
                session.exchange(request, TIMEOUT);
                throw new AssertionError("Interrupted request started");
            } catch (InterruptedIOException expected) {
                check(session.isOpen() && interrupted.outgoing.size() == 0, "Cancellation before I/O preserves session");
            } finally {
                Thread.interrupted();
            }
        }

        CountDownLatch reading = new CountDownLatch(1);
        CountDownLatch closed = new CountDownLatch(1);
        SerialChannel blocking = new SerialChannel() {
            @Override public int write(byte[] b, int o, int n, int ms) { return n; }
            @Override public int read(byte[] b, int o, int n, int ms) throws IOException {
                reading.countDown();
                try {
                    if (!closed.await(ms, TimeUnit.MILLISECONDS)) {
                        return 0;
                    }
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
                throw new IOException("Disconnected");
            }
            @Override public void close() { closed.countDown(); }
        };
        PacketSession busy = new PacketSession(blocking);
        AtomicReference<Throwable> firstResult = new AtomicReference<>();
        Thread first = new Thread(() -> {
            try {
                busy.exchange(request, TIMEOUT);
                firstResult.set(new AssertionError("Closed read succeeded"));
            } catch (Throwable expected) {
                firstResult.set(expected);
            }
        });
        first.start();
        try {
            check(reading.await(1, TimeUnit.SECONDS), "First transaction started");
            try {
                busy.exchange(request, Duration.ofMillis(50));
                throw new AssertionError("Concurrent exchange entered active transaction");
            } catch (SocketTimeoutException expected) {
                check(busy.isOpen(), "Queue timeout does not close active transaction");
            }
        } finally {
            busy.close();
            first.join(3000);
        }
        check(!first.isAlive() && firstResult.get() instanceof IOException, "Close releases blocked exchange");

        for (int writeResult : new int[] {0, -1, Integer.MAX_VALUE}) {
            CountDownLatch released = new CountDownLatch(1);
            SerialChannel brokenWriter = new SerialChannel() {
                @Override public int write(byte[] b, int o, int n, int ms) { return writeResult; }
                @Override public int read(byte[] b, int o, int n, int ms) {
                    throw new AssertionError("Read started after a failed write");
                }
                @Override public void close() { released.countDown(); }
            };
            try (PacketSession session = new PacketSession(brokenWriter)) {
                try {
                    session.exchange(request, TIMEOUT);
                    throw new AssertionError("Failed write was accepted");
                } catch (IOException expected) {
                    check(!session.isOpen() && released.getCount() == 0, "Write failure closes connection");
                }
            }
        }

        SerialChannel closeFailure = new SerialChannel() {
            @Override public int read(byte[] b, int o, int n, int ms) { return 0; }
            @Override public int write(byte[] b, int o, int n, int ms) { return n; }
            @Override public void close() throws IOException { throw new IOException("Close failed"); }
        };
        PacketSession failed = new PacketSession(closeFailure);
        try {
            failed.exchange(request, TIMEOUT);
            throw new AssertionError("Read timeout was accepted");
        } catch (SocketTimeoutException expected) {
            check(expected.getSuppressed().length == 1, "Original timeout preserves close failure");
            check(!failed.isOpen(), "Close failure still invalidates session");
        }
        System.out.println("Transport fragmentation, timeout, corruption, disconnect, cancellation and concurrency checks passed");
    }
}
