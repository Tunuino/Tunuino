/* Copyright (c) 2026 Tunuino. */
package io.tunuino.maintenance;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.time.Duration;
import java.util.List;
import java.util.concurrent.*;
import java.util.function.Consumer;

/** Argument vectors only: filenames and USB identifiers never pass through a shell. */
public final class CommandRunner {
    public record Result(int exitCode, String output) {}
    private CommandRunner() {}
    public static Result run(List<String> args, Path directory, Duration timeout,
                             Consumer<String> log) throws IOException {
        ProcessBuilder builder = new ProcessBuilder(args).redirectErrorStream(true);
        if (directory != null) builder.directory(directory.toFile());
        Process process = builder.start();
        process.getOutputStream().close();
        StringBuilder output = new StringBuilder();
        ExecutorService reader = Executors.newSingleThreadExecutor(r -> {
            Thread t = new Thread(r, "tunuino-programmer-output"); t.setDaemon(true); return t;
        });
        Future<?> drain = reader.submit(() -> {
            try (Reader input = new InputStreamReader(process.getInputStream(), StandardCharsets.UTF_8)) {
                char[] buffer = new char[2048];
                int count;
                while ((count = input.read(buffer)) != -1) {
                    String chunk = new String(buffer, 0, count);
                    synchronized (output) {
                        output.append(chunk);
                        if (output.length() > 262144) output.delete(0, output.length() - 262144);
                    }
                    log.accept(chunk);
                }
            } catch (IOException e) { throw new UncheckedIOException(e); }
        });
        try {
            if (!process.waitFor(timeout.toMillis(), TimeUnit.MILLISECONDS)) {
                throw new IOException("A ferramenta excedeu o tempo limite. Verifica o estado da ECU antes de repetir.");
            }
            drain.get(5, TimeUnit.SECONDS);
            synchronized (output) { return new Result(process.exitValue(), output.toString()); }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt(); throw new IOException("Operação interrompida", e);
        } catch (ExecutionException | TimeoutException e) {
            throw new IOException("Falha ao ler a saída da ferramenta", e);
        } finally {
            if (process.isAlive()) {
                process.descendants().forEach(ProcessHandle::destroyForcibly);
                process.destroyForcibly();
                try { process.waitFor(3, TimeUnit.SECONDS); }
                catch (InterruptedException e) { Thread.currentThread().interrupt(); }
            }
            reader.shutdownNow();
        }
    }
}
