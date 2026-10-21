/* Copyright (c) 2026 Tunuino. */
package io.tunuino.console;

import io.tunuino.datalog.CsvLogReader;
import io.tunuino.datalog.LogSample;
import java.io.File;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
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

    @Override
    public void start(Stage stage) {
        if (!System.getProperty("os.name", "").toLowerCase(Locale.ROOT).startsWith("windows")) {
            throw new IllegalStateException("Tunuino Console destina-se ao Windows");
        }
        stage.setTitle("Tunuino Console");
        ComboBox<String> hardware = new ComboBox<>();
        hardware.getItems().addAll("Proteus", "uaEFI");
        hardware.setValue("Proteus");
        Button open = new Button("Abrir datalog CSV");
        Label status = new Label("Offline · USB e gravação ainda em desenvolvimento");
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
        VBox footer = new VBox(8, summary, status);
        footer.setPadding(new Insets(12));
        BorderPane root = new BorderPane(chart);
        root.setTop(new ToolBar(new Label("Tunuino"), hardware, open));
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
    }

    @Override
    public void stop() {
        if (activeRead != null) {
            activeRead.cancel(true);
        }
        fileWorker.shutdownNow();
    }

    public static void main(String[] args) {
        launch(args);
    }
}
