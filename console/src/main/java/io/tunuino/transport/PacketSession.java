/* Copyright (c) 2026 Tunuino. */
package io.tunuino.transport;

import io.tunuino.protocol.FrameCodec;
import java.io.IOException;
import java.io.InterruptedIOException;
import java.net.SocketTimeoutException;
import java.time.Duration;
import java.util.Objects;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.locks.ReentrantLock;

/** One request/response at a time. A failed exchange invalidates the session. */
public final class PacketSession implements AutoCloseable {
    private final SerialChannel channel;
    private final ReentrantLock transaction = new ReentrantLock(true);
    private final AtomicBoolean closed = new AtomicBoolean();

    public PacketSession(SerialChannel channel) {
        this.channel = Objects.requireNonNull(channel, "channel");
    }

    public boolean isOpen() {
        return !closed.get();
    }

    public byte[] exchange(byte[] request, Duration timeout) throws IOException {
        byte[] packet = FrameCodec.encode(request);
        Objects.requireNonNull(timeout, "timeout");
        if (timeout.isNegative() || timeout.isZero() || timeout.compareTo(Duration.ofSeconds(60)) > 0) {
            throw new IllegalArgumentException("Timeout deve estar entre 1 ns e 60 s");
        }
        long duration = timeout.toNanos();
        long deadline = System.nanoTime() + duration;
        boolean acquired;
        try {
            acquired = transaction.tryLock(duration, TimeUnit.NANOSECONDS);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new InterruptedIOException("Pedido cancelado antes de iniciar");
        }
        if (!acquired) {
            throw new SocketTimeoutException("A sessão está ocupada");
        }
        try {
            if (closed.get()) {
                throw new IOException("Sessão encerrada");
            }
            transfer(packet, 0, packet.length, deadline, true);
            byte[] header = new byte[2];
            transfer(header, 0, 2, deadline, false);
            int length = ((header[0] & 255) << 8) | (header[1] & 255);
            if (length < 1 || length > FrameCodec.MAX_BODY) {
                throw new IOException("Comprimento de resposta inválido: " + length);
            }
            byte[] response = new byte[length + 6];
            System.arraycopy(header, 0, response, 0, 2);
            transfer(response, 2, length + 4, deadline, false);
            remainingMs(deadline);
            try {
                return FrameCodec.decode(response);
            } catch (IllegalArgumentException e) {
                throw new IOException(e.getMessage(), e);
            }
        } catch (IOException | RuntimeException e) {
            try {
                close();
            } catch (IOException closeFailure) {
                e.addSuppressed(closeFailure);
            }
            throw e;
        } finally {
            transaction.unlock();
        }
    }

    private void transfer(byte[] buffer, int offset, int count, long deadline, boolean write) throws IOException {
        int completed = 0;
        while (completed < count) {
            if (Thread.currentThread().isInterrupted()) {
                throw new InterruptedIOException("Pedido cancelado");
            }
            if (closed.get()) {
                throw new IOException("Sessão encerrada");
            }
            int timeout = remainingMs(deadline);
            int remaining = count - completed;
            int n = write ? channel.write(buffer, offset + completed, remaining, timeout)
                          : channel.read(buffer, offset + completed, remaining, timeout);
            if (n == 0) {
                throw new SocketTimeoutException("Tempo de comunicação excedido");
            }
            if (n < 0 || n > remaining) {
                throw new IOException("Ligação interrompida ou contagem de bytes inválida");
            }
            completed += n;
        }
    }

    private static int remainingMs(long deadline) throws SocketTimeoutException {
        long nanos = deadline - System.nanoTime();
        if (nanos <= 0) {
            throw new SocketTimeoutException("Tempo total do pedido excedido");
        }
        return (int) Math.min(60_000L, (nanos + 999_999L) / 1_000_000L);
    }

    @Override
    public void close() throws IOException {
        if (closed.compareAndSet(false, true)) {
            channel.close();
        }
    }
}
