/* Copyright (c) 2026 Tunuino. */
package io.tunuino.console;

import io.tunuino.datalog.CsvLogReader;
import io.tunuino.datalog.LogSample;
import io.tunuino.transport.PacketSession;
import io.tunuino.transport.UsbSerialPorts;
import io.tunuino.protocol.DeviceIdentity;
import io.tunuino.protocol.IdentityClient;
import java.io.File;
import java.time.Duration;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Callable;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;
import java.util.function.Consumer;
import javafx.application.Application;
import javafx.concurrent.Task;
import javafx.geometry.Insets;
import javafx.scene.Scene;
import javafx.scene.chart.LineChart;
import javafx.scene.chart.NumberAxis;
import javafx.scene.chart.XYChart;
import javafx.scene.control.Alert;
import javafx.scene.control.Button;
import javafx.scene.control.ComboBox;
import javafx.scene.control.Label;
import javafx.scene.control.ToolBar;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.VBox;
import javafx.stage.FileChooser;
import javafx.stage.Stage;

public final class TunuinoConsole extends Application {
    private final ExecutorService fileWorker = Executors.newSingleThreadExecutor(r -> {
        Thread thread = new Thread(r, "tunuino-file-reader");
        thread.setDaemon(true);
        return thread;
    });
    private Task<List<LogSample>> activeRead;
    private final AtomicBoolean stopping = new AtomicBoolean();
    private final AtomicReference<PacketSession> connection = new AtomicReference<>();
    private final ExecutorService serialWorker = Executors.newSingleThreadExecutor(r -> {
        Thread thread = new Thread(r, "tunuino-usb");
        thread.setDaemon(true);
        return thread;
    });

    private <T> void serialTask(Callable<T> operation, Consumer<T> success, Consumer<Throwable> failure) {
        Task<T> task = new Task<>() {
            @Override protected T call() throws Exception {
                return operation.call();
            }
        };
        task.setOnSucceeded(event -> {
            if (!stopping.get()) {
                success.accept(task.getValue());
            }
        });
        task.setOnFailed(event -> {
            if (!stopping.get()) {
                failure.accept(task.getException());
            }
        });
        serialWorker.submit(task);
    }

    @Override
    public void start(Stage stage) {
        if (!System.getProperty("os.name", "").toLowerCase(Locale.ROOT).startsWith("windows")) {
            throw new IllegalStateException("Tunuino Console destina-se ao Windows");
        }
        stage.setTitle("Tunuino Console");
        ComboBox<DeviceIdentity.Family> hardware = new ComboBox<>();
        hardware.getItems().addAll(DeviceIdentity.Family.values());
        hardware.setValue(DeviceIdentity.Family.PROTEUS);
        Button open = new Button("Abrir datalog CSV");
        Label status = new Label("Offline · nenhuma ECU identificada");
        Label identityDetails = new Label("Identificação da ECU pendente");
        ComboBox<UsbSerialPorts.Port> ports = new ComboBox<>();
        ports.setPromptText("Selecionar porta USB");
        ports.setPrefWidth(290);
        Button scan = new Button("Atualizar portas");
        Button connect = new Button("Ligar USB");
        Button identify = new Button("Identificar ECU");
        identify.setDisable(true);
        connect.setDisable(true);
        Runnable refreshConnection = () -> {
            PacketSession current = connection.get();
            boolean connected = current != null && current.isOpen();
            connect.setText(connected ? "Desligar USB" : "Ligar USB");
            connect.setDisable(!connected && ports.getValue() == null);
            scan.setDisable(connected);
            ports.setDisable(connected);
            hardware.setDisable(connected);
            identify.setDisable(!connected);
        };
        Consumer<Throwable> usbFailure = error -> {
            identityDetails.setText("Identificação da ECU pendente");
            status.setText("Falha USB: " + error.getMessage());
            refreshConnection.run();
        };
        ports.setOnAction(event -> refreshConnection.run());
        scan.setOnAction(event -> {
            scan.setDisable(true);
            connect.setDisable(true);
            ports.setDisable(true);
            identify.setDisable(true);
            status.setText("A procurar portas…");
            serialTask(UsbSerialPorts::list, found -> {
                ports.getItems().setAll(found);
                ports.setValue(null);
                status.setText(found.isEmpty() ? "Nenhuma porta encontrada" : "Seleciona a porta da centralina");
                refreshConnection.run();
            }, usbFailure);
        });
        connect.setOnAction(event -> {
            identityDetails.setText("Identificação da ECU pendente");
            identify.setDisable(true);
            connect.setDisable(true);
            scan.setDisable(true);
            ports.setDisable(true);
            hardware.setDisable(true);
            UsbSerialPorts.Port selected = ports.getValue();
            PacketSession current = connection.get();
            if (current != null && current.isOpen()) {
                serialTask(() -> {
                    PacketSession old = connection.getAndSet(null);
                    if (old != null) {
                        old.close();
                    }
                    return Boolean.TRUE;
                }, ignored -> {
                    status.setText("Offline · ligação encerrada");
                    refreshConnection.run();
                }, usbFailure);
            } else if (selected != null) {
                status.setText("A abrir " + selected.name() + "…");
                serialTask(() -> {
                    PacketSession old = connection.getAndSet(null);
                    if (old != null) {
                        old.close();
                    }
                    PacketSession opened = UsbSerialPorts.open(selected);
                    if (stopping.get()) {
                        opened.close();
                        return Boolean.FALSE;
                    }
                    connection.set(opened);
                    // stop() may race with completion of the native open operation.
                    if (stopping.get()) {
                        PacketSession pending = connection.getAndSet(null);
                        if (pending != null) {
                            pending.close();
                        }
                        return Boolean.FALSE;
                    }
                    return Boolean.TRUE;
                }, opened -> {
                    status.setText("Porta " + selected.name() + " aberta · ECU ainda por identificar");
                    refreshConnection.run();
                }, usbFailure);
            } else {
                refreshConnection.run();
            }
        });
        identify.setOnAction(event -> {
            PacketSession current = connection.get();
            if (current == null || !current.isOpen()) {
                refreshConnection.run();
                return;
            }
            DeviceIdentity.Family expected = hardware.getValue();
            identify.setDisable(true);
            connect.setDisable(true);
            status.setText("A identificar a ECU…");
            identityDetails.setText("Identificação da ECU pendente");
            serialTask(() -> {
                try {
                    return IdentityClient.identify(current, expected, Duration.ofSeconds(3));
                } catch (java.io.IOException error) {
                    connection.compareAndSet(current, null);
                    throw error;
                }
            }, identity -> {
                identityDetails.setText(identity.family() + " · " + identity.mcuSeries()
                    + " · revisão " + identity.hardwareRevision()
                    + " · firmware " + identity.firmwareVersion() + " · UID " + identity.uid());
                status.setText("ECU Tunuino reconhecida pelo protocolo de identificação");
                refreshConnection.run();
            }, usbFailure);
        });
        NumberAxis time = new NumberAxis();
        time.setLabel("Tempo (s)");
        time.setForceZeroInRange(false);
        NumberAxis rpm = new NumberAxis();
        rpm.setLabel("RPM");
        LineChart<Number, Number> chart = new LineChart<>(time, rpm);
        chart.setAnimated(false);
        chart.setCreateSymbols(false);
        chart.setLegendVisible(false);
        chart.setTitle("Datalog offline");
        Label summary = new Label("Escolhe um datalog local para analisar.");
        identityDetails.setWrapText(true);
        VBox footer = new VBox(8, summary, identityDetails, status);
        footer.setPadding(new Insets(12));
        BorderPane root = new BorderPane(chart);
        root.setTop(new VBox(new ToolBar(new Label("Tunuino"), hardware, open),
            new ToolBar(ports, scan, connect, identify)));
        root.setBottom(footer);
        open.setOnAction(event -> {
            FileChooser chooser = new FileChooser();
            chooser.setTitle("Abrir datalog Tunuino");
            chooser.getExtensionFilters().add(new FileChooser.ExtensionFilter("Datalog CSV", "*.csv"));
            File selected = chooser.showOpenDialog(stage);
            if (selected == null) {
                return;
            }
            open.setDisable(true);
            summary.setText("A ler " + selected.getName() + "…");
            activeRead = new Task<>() {
                @Override protected List<LogSample> call() throws Exception {
                    return CsvLogReader.read(selected.toPath());
                }
            };
            activeRead.setOnSucceeded(done -> {
                List<LogSample> samples = activeRead.getValue();
                XYChart.Series<Number, Number> series = new XYChart.Series<>();
                int stride = Math.max(1, (samples.size() + 1999) / 2000);
                for (int i = 0; i < samples.size(); i += stride) {
                    LogSample s = samples.get(i);
                    series.getData().add(new XYChart.Data<>(s.timeMs() / 1000.0, s.rpm()));
                }
                if ((samples.size() - 1) % stride != 0) {
                    LogSample s = samples.get(samples.size() - 1);
                    series.getData().add(new XYChart.Data<>(s.timeMs() / 1000.0, s.rpm()));
                }
                chart.getData().clear();
                chart.getData().add(series);
                double maxRpm = samples.stream().mapToDouble(LogSample::rpm).max().orElse(0);
                summary.setText(String.format(Locale.ROOT,
                    "%s · %d amostras · RPM máximo %.0f · gráfico reduzido a %d pontos",
                    selected.getName(), samples.size(), maxRpm, series.getData().size()));
                open.setDisable(false);
            });
            activeRead.setOnFailed(done -> {
                summary.setText("Não foi possível abrir o datalog. O gráfico anterior foi mantido.");
                Alert alert = new Alert(Alert.AlertType.ERROR, activeRead.getException().getMessage());
                alert.initOwner(stage);
                alert.setHeaderText("Datalog inválido");
                alert.showAndWait();
                open.setDisable(false);
            });
            fileWorker.submit(activeRead);
        });
        stage.setScene(new Scene(root, 1060, 650));
        stage.show();
        scan.fire();
    }

    @Override
    public void stop() {
        if (!stopping.compareAndSet(false, true)) {
            return;
        }
        if (activeRead != null) {
            activeRead.cancel(true);
        }
        fileWorker.shutdownNow();
        serialWorker.execute(() -> {
            PacketSession current = connection.getAndSet(null);
            if (current != null) {
                try {
                    current.close();
                } catch (java.io.IOException e) {
                    System.err.println("Tunuino: " + e.getMessage());
                }
            }
        });
        serialWorker.shutdown();
    }

    public static void main(String[] args) {
        launch(args);
    }
}
