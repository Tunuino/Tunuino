/* Copyright (c) 2026 Tunuino. */
package io.tunuino.transport;

import java.io.IOException;
import java.io.InterruptedIOException;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.List;
import java.util.Objects;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;

/** Local simulator pipe transport. This class never opens a USB device or network socket. */
public final class ProcessSerialChannel implements SerialChannel {
    private final Process process;
    private final AtomicBoolean closed = new AtomicBoolean();
    private final ExecutorService writes = Executors.newSingleThreadExecutor(task -> {
        Thread thread = new Thread(task, "tunuino-simulator-write"); thread.setDaemon(true); return thread;
    });

    public ProcessSerialChannel(List<String> command) throws IOException {
        if (command.isEmpty()) throw new IllegalArgumentException("Missing simulator executable");
        process = new ProcessBuilder(List.copyOf(command)).redirectError(ProcessBuilder.Redirect.INHERIT).start();
    }

    public static ProcessSerialChannel simulator(Path executable, String family, String scenario) throws IOException {
        if (!List.of("proteus", "uaefi").contains(family)
                || !List.of("normal", "stall", "sensor-loss", "wrap").contains(scenario)) {
            throw new IllegalArgumentException("Invalid simulator profile");
        }
        return new ProcessSerialChannel(List.of(executable.toAbsolutePath().toString(), "--stdio", "--family", family,
            "--scenario", scenario));
    }

    private static void check(byte[] buffer, int offset, int count, int timeoutMs) {
        Objects.checkFromIndexSize(offset, count, buffer.length);
        if (timeoutMs < 1 || timeoutMs > 60000 || count > 4102) throw new IllegalArgumentException("Invalid pipe operation");
    }

    @Override public int read(byte[] buffer, int offset, int count, int timeoutMs) throws IOException {
        check(buffer, offset, count, timeoutMs);
        long deadline = System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(timeoutMs);
        while (!closed.get()) {
            int ready = process.getInputStream().available();
            if (ready > 0) return process.getInputStream().read(buffer, offset, Math.min(count, ready));
            if (!process.isAlive()) return -1;
            if (System.nanoTime() - deadline >= 0) return 0;
            try { Thread.sleep(1); }
            catch (InterruptedException error) {
                Thread.currentThread().interrupt(); throw new InterruptedIOException("Simulator read cancelled");
            }
        }
        return -1;
    }

    @Override public int write(byte[] buffer, int offset, int count, int timeoutMs) throws IOException {
        check(buffer, offset, count, timeoutMs);
        if (closed.get()) return -1;
        byte[] copy = Arrays.copyOfRange(buffer, offset, offset + count);
        var pending = writes.submit(() -> {
            process.getOutputStream().write(copy); process.getOutputStream().flush(); return count;
        });
        try { return pending.get(timeoutMs, TimeUnit.MILLISECONDS); }
        catch (InterruptedException error) {
            pending.cancel(true); process.destroyForcibly(); Thread.currentThread().interrupt();
            throw new InterruptedIOException("Simulator write cancelled");
        } catch (TimeoutException error) {
            pending.cancel(true); process.destroyForcibly();
            throw new java.net.SocketTimeoutException("Simulator write timed out");
        } catch (ExecutionException error) {
            throw new IOException("Simulator pipe failed", error.getCause());
        }
    }

    public boolean isAlive() { return process.isAlive(); }

    @Override public void close() throws IOException {
        if (!closed.compareAndSet(false, true)) return;
        writes.shutdownNow(); process.destroyForcibly();
        try {
            if (!process.waitFor(1, TimeUnit.SECONDS)) throw new IOException("Simulator process did not stop");
        } catch (InterruptedException error) {
            Thread.currentThread().interrupt(); throw new InterruptedIOException("Simulator cleanup cancelled");
        } finally {
            try { process.getInputStream().close(); } finally { process.getOutputStream().close(); }
        }
    }
}
