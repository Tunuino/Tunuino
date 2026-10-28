/* Copyright (c) 2026 Tunuino. */
package io.tunuino.datalog;

import io.tunuino.protocol.DeviceIdentity;
import io.tunuino.protocol.LiveDataClient;
import io.tunuino.transport.PacketSession;
import java.io.IOException;
import java.io.InterruptedIOException;
import java.nio.file.Path;
import java.time.Duration;
import java.util.Objects;
import java.util.Optional;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;

/** One polling worker owns the serial session and all recording file operations. */
public final class LiveDataMonitor {
    public record Recording(Path path, long samples) {}
    public record Extras(io.tunuino.protocol.AuxControlClient.FeatureState sensors,io.tunuino.protocol.EngineOutputClient.State outputs,byte[] profile,io.tunuino.protocol.CamPhaseClient.Phase cam) {
        public Extras(io.tunuino.protocol.AuxControlClient.FeatureState sensors,io.tunuino.protocol.EngineOutputClient.State outputs,byte[] profile){this(sensors,outputs,profile,null);}
        public Extras {profile=profile==null?null:profile.clone();}
        @Override public byte[] profile(){return profile==null?null:profile.clone();}
    }
    public record TuningFrame(Optional<LogSample> sample,Extras extras,long responseMs) {}
    private volatile Consumer<TuningFrame> tuning;
    /** Observer runs on the serial worker; it may not perform serial operations. */
    public void tuningObserver(Consumer<TuningFrame> observer){tuning=observer;}
    private record Request(Path destination, CompletableFuture<Recording> completion) {}
    private final PacketSession session;
    private final DeviceIdentity identity;
    private final Duration interval;
    private final ConcurrentLinkedQueue<Request> requests = new ConcurrentLinkedQueue<>();
    private volatile Recording recording;
    private CsvLogWriter writer;
    private boolean started;
    private boolean finished;

    public LiveDataMonitor(PacketSession session, DeviceIdentity identity, Duration interval) {
        this.session = Objects.requireNonNull(session, "session");
        this.identity = Objects.requireNonNull(identity, "identity");
        this.interval = Objects.requireNonNull(interval, "interval");
        if ((identity.capabilities() & LiveDataClient.CAPABILITY) == 0) {
            throw new IllegalStateException("O firmware não anuncia dados em tempo real");
        }
        if (interval.compareTo(Duration.ofMillis(10)) < 0 || interval.compareTo(Duration.ofSeconds(5)) > 0) {
            throw new IllegalArgumentException("Intervalo deve estar entre 10 ms e 5 s");
        }
    }

    public Recording recording() { return recording; }

    /** These requests only enqueue work; they never access the filesystem on the UI thread. */
    public synchronized CompletableFuture<Recording> startRecording(Path destination) {
        return enqueue(Objects.requireNonNull(destination, "destination").toAbsolutePath());
    }

    public synchronized CompletableFuture<Recording> stopRecording() { return enqueue(null); }

    private CompletableFuture<Recording> enqueue(Path destination) {
        CompletableFuture<Recording> completion = new CompletableFuture<>();
        if (finished) completion.completeExceptionally(new IOException("Monitorização terminada"));
        else requests.add(new Request(destination, completion));
        return completion;
    }

    private Recording finishRecording() throws IOException {
        Recording result = recording;
        CsvLogWriter closing = writer;
        writer = null;
        recording = null;
        if (closing != null) closing.close();
        return result;
    }

    private void processRequests() {
        Request request;
        while ((request = requests.poll()) != null) {
            try {
                Recording result;
                if (request.destination() == null) {
                    result = finishRecording();
                } else {
                    if (writer != null) throw new IOException("Já existe um datalog em gravação");
                    writer = new CsvLogWriter(request.destination(), identity.family() == DeviceIdentity.Family.PROTEUS);
                    result = new Recording(request.destination(), 0);
                    recording = result;
                }
                request.completion().complete(result);
            } catch (IOException error) {
                request.completion().completeExceptionally(error);
            }
        }
    }

    /** Empty samples mean unavailable data, never zero-valued measurements. */
    public long run(BooleanSupplier running, Consumer<Optional<LogSample>> samples,
            Consumer<IOException> recordingErrors) throws IOException {
        return run(running,samples,recordingErrors,null);
    }
    public long run(BooleanSupplier running, Consumer<Optional<LogSample>> samples,
            Consumer<IOException> recordingErrors, Consumer<Extras> extras) throws IOException {
        Objects.requireNonNull(running, "running");
        Objects.requireNonNull(samples, "samples");
        Objects.requireNonNull(recordingErrors, "recordingErrors");
        synchronized (this) {
            if (started) throw new IllegalStateException("Monitorização já iniciada");
            started = true;
        }
        long count = 0;
        long previousTime = -1;
        long nextExtras=0;long profileGeneration=-1;byte[] outputProfile=null;
        boolean extraSupported=extras!=null&&identity.capabilities()==15&&identity.firmwareVersion().equals("0.2.0");
        boolean camSupported=extraSupported;
        try {
            while (running.getAsBoolean()) {
                processRequests();
                Optional<LogSample> result = LiveDataClient.read(session, identity, Duration.ofMillis(500));
                long receivedAt=System.nanoTime();
                if (result.isPresent()) {
                    LogSample sample = result.get();
                    if (sample.timeMs() < previousTime) {
                        IOException error = new IOException("O relógio da ECU recuou; volta a identificar a ECU");
                        try { session.close(); } catch (IOException closing) { error.addSuppressed(closing); }
                        throw error;
                    }
                    previousTime = sample.timeMs();
                    ++count;
                    if (writer != null) {
                        try {
                            writer.append(sample);
                            recording = new Recording(recording.path(), recording.samples() + 1);
                        } catch (IOException error) {
                            try { finishRecording(); } catch (IOException closing) { error.addSuppressed(closing); }
                            recordingErrors.accept(error);
                        }
                    }
                }
                samples.accept(result);
                if(extraSupported&&running.getAsBoolean()&&System.nanoTime()>=nextExtras){
                    var sensors=io.tunuino.protocol.AuxControlClient.optionalFeatures(session);
                    if(sensors.isEmpty())extraSupported=false;
                    else {
                        var state=io.tunuino.protocol.EngineOutputClient.read(session);
                        if(profileGeneration!=state.generation()){
                            var configuration=io.tunuino.protocol.ConfigurationClient.read(session,identity,Duration.ofSeconds(3));
                            outputProfile=!configuration.validation().dirty()&&configuration.validation().generation()==state.generation()?configuration.configuration().engineProfile():null;
                            profileGeneration=outputProfile==null?-1:state.generation();
                        }
                        io.tunuino.protocol.CamPhaseClient.Phase cam=null;
                        if(camSupported){
                            var phase=io.tunuino.protocol.CamPhaseClient.optional(session);
                            camSupported=phase.isPresent();
                            if(phase.isPresent()&&phase.get().generation()==state.generation())cam=phase.get();
                        }
                        var extra=new Extras(sensors.get(),state,outputProfile,cam);
                        extras.accept(extra);
                        var observer=tuning;if(observer!=null)observer.accept(new TuningFrame(result,extra,
                            Duration.ofNanos(System.nanoTime()-receivedAt).toMillis()));
                    }
                    nextExtras=System.nanoTime()+Duration.ofMillis(tuning==null?500:0).toNanos();
                }
                if (running.getAsBoolean()) {
                    try {
                        long nanos = interval.toNanos();
                        Thread.sleep(nanos / 1_000_000, (int) (nanos % 1_000_000));
                    } catch (InterruptedException error) {
                        Thread.currentThread().interrupt();
                        throw new InterruptedIOException("Monitorização cancelada");
                    }
                }
            }
            return count;
        } finally {
            synchronized (this) { finished = true; }
            try { finishRecording(); } catch (IOException error) { recordingErrors.accept(error); }
            Request request;
            while ((request = requests.poll()) != null) {
                request.completion().completeExceptionally(new IOException("Monitorização terminada"));
            }
        }
    }
}
