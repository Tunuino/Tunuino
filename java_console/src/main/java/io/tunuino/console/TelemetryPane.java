/* Copyright (c) 2026 Tunuino. */
package io.tunuino.console;

import io.tunuino.datalog.DatalogAnalysis;
import io.tunuino.datalog.LogChannel;
import io.tunuino.datalog.LogSample;
import io.tunuino.protocol.IoClient;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.EnumMap;
import java.util.List;
import java.util.Locale;
import java.util.Optional;
import javafx.animation.KeyFrame;
import javafx.animation.Timeline;
import javafx.geometry.Insets;
import javafx.scene.chart.LineChart;
import javafx.scene.chart.NumberAxis;
import javafx.scene.chart.XYChart;
import javafx.scene.control.Button;
import javafx.scene.control.ComboBox;
import javafx.scene.control.Label;
import javafx.scene.control.Slider;
import javafx.scene.control.ToolBar;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.FlowPane;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;
import javafx.scene.layout.VBox;
import javafx.util.Duration;

/** All methods are called on the JavaFX application thread. */
public final class TelemetryPane extends BorderPane {
    private static final int LIVE_LIMIT = 600;
    private final EnumMap<LogChannel, Label> values = new EnumMap<>(LogChannel.class);
    private final EnumMap<LogChannel, Label> ranges = new EnumMap<>(LogChannel.class);
    private final ComboBox<LogChannel> channel = new ComboBox<>();
    private final NumberAxis vertical = new NumberAxis();
    private final LineChart<Number, Number> chart;
    private final Slider cursor = new Slider(0, 1, 0);
    private final Button previous = new Button("Anterior");
    private final Button next = new Button("Seguinte");
    private final Button play = new Button("Reproduzir");
    private final ComboBox<Double> speed = new ComboBox<>();
    private final Label source = new Label("Sem dados · abre um datalog ou liga a ECU");
    private final Label position = new Label("Nenhuma amostra selecionada");
    private final ArrayDeque<Optional<LogSample>> liveSamples = new ArrayDeque<>();
    private final Timeline playback;
    private List<LogSample> offline = List.of();
    private boolean live;
    private double playbackTime;
    private long playbackTick;
    private final Button rawInputs = new Button("Entradas ADC");
    private final Label[] rawValues = new Label[16];
    private IoClient.Inputs currentInputs;
    private javafx.stage.Stage inputWindow;
    private final Label inputState = new Label("Sem dados atuais");

    public TelemetryPane() {
        getStyleClass().add("telemetry-pane");
        source.getStyleClass().add("telemetry-source");
        position.getStyleClass().add("datalog-position");
        FlowPane cards = new FlowPane(12, 10);
        for (LogChannel item : LogChannel.values()) {
            Label value = new Label("—");
            value.setId("telemetry-value-" + item.name().toLowerCase(Locale.ROOT));
            value.getStyleClass().add("telemetry-value");
            Label range = new Label("Sem amostra");
            range.getStyleClass().add("telemetry-range");
            values.put(item, value); ranges.put(item, range);
            Label title = new Label((item == LogChannel.COOLANT ? "Temperatura" : item) + " · " + item.unit());
            title.getStyleClass().add("telemetry-card-title");
            VBox card = new VBox(6, title, value, range);
            card.setMinWidth(150); card.setPrefWidth(192);
            card.getStyleClass().addAll("telemetry-card", item.name().toLowerCase(Locale.ROOT));
            cards.getChildren().add(card);
        }
        cards.widthProperty().addListener((observable, before, after) -> {
            double width = Math.max(0, after.doubleValue());
            int columns = width >= 660 ? 4 : 2;
            double cardWidth = Math.max(150, Math.floor((width - (columns - 1) * 12 - 4) / columns));
            cards.getChildren().forEach(node -> ((VBox) node).setPrefWidth(cardWidth));
        });
        channel.getItems().setAll(LogChannel.values()); channel.setValue(LogChannel.RPM);
        channel.setId("telemetry-channel");
        NumberAxis time = new NumberAxis();
        time.setLabel("Tempo desde a primeira amostra (s)"); time.setForceZeroInRange(false);
        vertical.setForceZeroInRange(false);
        chart = new LineChart<>(time, vertical);
        chart.setId("telemetry-chart");
        chart.setAnimated(false); chart.setCreateSymbols(false); chart.setLegendVisible(false);
        chart.setTitle("Dados e datalogs");
        chart.setMinHeight(160);
        channel.setOnAction(event -> renderChart());
        rawInputs.setId("io-raw-inputs");rawInputs.setDisable(true);rawInputs.setOnAction(event -> openInputs());
        VBox top = new VBox(10, source, cards, new ToolBar(new Label("Canal do gráfico"), channel, rawInputs));
        top.getStyleClass().add("telemetry-top");
        setTop(top); setCenter(chart);
        cursor.setId("telemetry-cursor");
        cursor.setBlockIncrement(1);
        cursor.valueProperty().addListener((property, before, after) -> inspect());
        cursor.setOnMousePressed(event -> stopPlayback());
        cursor.setOnKeyPressed(event -> stopPlayback());
        previous.setId("telemetry-previous"); next.setId("telemetry-next"); play.setId("telemetry-play");
        previous.setOnAction(event -> { stopPlayback(); cursor.setValue(index() - 1); });
        next.setOnAction(event -> { stopPlayback(); cursor.setValue(index() + 1); });
        speed.getItems().setAll(0.25, 1.0, 2.0, 5.0, 10.0); speed.setValue(1.0);
        playback = new Timeline(new KeyFrame(Duration.millis(100), event -> advancePlayback()));
        playback.setCycleCount(Timeline.INDEFINITE);
        play.setOnAction(event -> {
            if (playback.getStatus() == Timeline.Status.RUNNING) { stopPlayback(); return; }
            if (offline.size() < 2 || live) return;
            if (index() == offline.size() - 1) cursor.setValue(0);
            playbackTime = offline.get(index()).timeMs() - offline.get(0).timeMs();
            playbackTick = System.nanoTime();
            playback.play(); play.setText("Pausar");
        });
        HBox navigation = new HBox(8, previous, play, next, speed, cursor);
        navigation.setAlignment(javafx.geometry.Pos.CENTER_LEFT);
        HBox.setHgrow(cursor, Priority.ALWAYS);
        VBox bottom = new VBox(8, navigation, position); bottom.setPadding(new Insets(8, 0, 0, 0));
        setBottom(bottom);
        refreshNavigation(); renderChart();
    }

    public void showOffline(String filename, List<LogSample> samples) {
        if (samples.isEmpty()) throw new IllegalArgumentException("O datalog não contém amostras");
        stopPlayback(); live = false; liveSamples.clear(); offline = List.copyOf(samples);
        source.setText("Datalog local · " + filename + " · " + samples.size() + " amostras");
        for (LogChannel item : LogChannel.values()) {
            DatalogAnalysis.Range range = DatalogAnalysis.range(offline, item);
            ranges.get(item).setText(Double.isNaN(range.minimum())?"Sem dados válidos":String.format(Locale.ROOT, "Mín. %.3f · máx. %.3f", range.minimum(), range.maximum()));
        }
        cursor.setMax(Math.max(1, offline.size() - 1)); cursor.setValue(0);
        renderChart(); inspect(); refreshNavigation();
    }

    public void beginLive() {
        stopPlayback(); live = true; offline = List.of(); liveSamples.clear();
        source.setText("Monitorização USB · à espera de amostras válidas");
        unavailable(); renderChart(); refreshNavigation();
    }

    public void showLive(Optional<LogSample> sample) {
        if (!live) return;
        // Consecutive missing readings need just one gap in the chart.
        if (sample.isPresent() || liveSamples.isEmpty() || liveSamples.getLast().isPresent()) {
            liveSamples.addLast(sample);
            while (liveSamples.size() > LIVE_LIMIT) liveSamples.removeFirst();
        }
        if (sample.isPresent()) {
            LogSample value = sample.get();
            showValues(value);
            for (LogChannel item : LogChannel.values()) ranges.get(item).setText(item.available(value)?"Última amostra válida":"Sem dados atuais");
            source.setText("Monitorização USB · "+Integer.bitCount(value.validMask())+"/4 sinais válidos");
            position.setText(String.format(Locale.ROOT, "Relógio ECU: %.3f s", value.timeMs() / 1000.0));
        } else {
            unavailable(); source.setText("Monitorização USB · sem dados válidos");
        }
        renderChart();
    }

    public void endLive(String reason) {
        live = false; stopPlayback(); unavailable();
        currentInputs=null;rawInputs.setDisable(true);refreshInputs();
        source.setText(reason); refreshNavigation();
    }

    public void showInputs(IoClient.Inputs value) {
        currentInputs=value;rawInputs.setDisable(value==null);refreshInputs();
    }

    private void refreshInputs() {
        IoClient.Inputs value=currentInputs;
        inputState.setText(value==null?"Sem dados atuais":"Configuração "+value.generation()+" · erros de captura "+value.errors()
            +((value.status()&2)!=0?" · alterações por aplicar":"")+((value.status()&4)!=0?" · guardada na ECU":""));
        int[] raw=value==null?null:value.raw();
        for(int i=0;i<16;++i)if(rawValues[i]!=null){
            String name=i<11?"AV"+(i+1):i==11?"12 V dividido (MCU)":"AT"+(i-11);
            rawValues[i].setText(name+"\n"+(value==null||(value.status()&1)==0?"—":String.format(Locale.ROOT,"%d ADC · %.3f V",raw[i],raw[i]*value.referenceMv()/4095000.0)));
        }
    }

    private void openInputs() {
        if(inputWindow!=null&&inputWindow.isShowing()){inputWindow.toFront();return;}
        inputWindow=new javafx.stage.Stage();inputWindow.initOwner(getScene().getWindow());
        inputWindow.setTitle("Tunuino · entradas analógicas da ECU");ConsoleBranding.applyIcon(inputWindow);
        FlowPane fields=new FlowPane(12,12);
        for(int i=0;i<16;++i){rawValues[i]=new Label();rawValues[i].setId("adc-value-"+(i+1));rawValues[i].setMinWidth(180);rawValues[i].setPadding(new Insets(8));fields.getChildren().add(rawValues[i]);}
        Label note=new Label("Tensão nos pinos ADC do MCU. A escala dos divisores externos depende do circuito da entrada.");note.setWrapText(true);
        VBox content=new VBox(12,note,fields,inputState);content.setPadding(new Insets(16));
        content.getStyleClass().add("workspace");
        BorderPane root=new BorderPane(ConsoleShell.scroll(content));root.getStyleClass().add("workspace");
        inputWindow.setScene(new javafx.scene.Scene(root,680,470));
        ConsoleBranding.applyTheme(inputWindow.getScene());refreshInputs();inputWindow.show();
    }

    private void unavailable() {
        for (Label value : values.values()) value.setText("—");
        for (Label range : ranges.values()) range.setText("Sem dados atuais");
        position.setText("Sem amostra atual");
    }

    private void showValues(LogSample sample) {
        for (LogChannel item : LogChannel.values()) values.get(item).setText(item.format(sample));
    }

    private int index() { return Math.max(0, Math.min(offline.size() - 1, (int) Math.round(cursor.getValue()))); }

    private void inspect() {
        if (live || offline.isEmpty()) return;
        int selected = index(); LogSample sample = offline.get(selected);
        showValues(sample);
        position.setText(String.format(Locale.ROOT, "Amostra %d / %d · tempo %.3f s · relógio ECU %.3f s",
            selected + 1, offline.size(), (sample.timeMs() - offline.get(0).timeMs()) / 1000.0,
            sample.timeMs() / 1000.0));
        refreshNavigation();
    }

    private void refreshNavigation() {
        boolean absent = live || offline.isEmpty();
        cursor.setDisable(absent || offline.size() < 2);
        previous.setDisable(absent || index() == 0);
        next.setDisable(absent || index() >= offline.size() - 1);
        play.setDisable(absent || offline.size() < 2); speed.setDisable(absent || offline.size() < 2);
    }

    private void renderChart() {
        LogChannel selected = channel.getValue();
        chart.getStyleClass().removeAll("channel-map", "channel-lambda", "channel-coolant", "channel-rpm");
        chart.getStyleClass().add("channel-" + selected.name().toLowerCase(Locale.ROOT));
        vertical.setLabel(selected + " (" + selected.unit() + ")");
        List<XYChart.Series<Number, Number>> series = new ArrayList<>();
        if (!offline.isEmpty()) {
            XYChart.Series<Number, Number> trace = null;
            long origin = offline.get(0).timeMs();
            for (LogSample sample : DatalogAnalysis.chartSamples(offline, selected, 2000)) {
                if(!selected.available(sample)){trace=null;continue;}
                if(trace==null){trace=new XYChart.Series<>();series.add(trace);}
                trace.getData().add(new XYChart.Data<>((sample.timeMs() - origin) / 1000.0, selected.value(sample)));
            }
            chart.setTitle("Datalog local · " + selected);
        } else {
            XYChart.Series<Number, Number> trace = null;
            long origin = liveSamples.stream().flatMap(Optional::stream).findFirst().map(LogSample::timeMs).orElse(0L);
            for (Optional<LogSample> entry : liveSamples) {
                if (entry.isEmpty()) { trace = null; continue; }
                if(!selected.available(entry.get())){trace=null;continue;}
                if (trace == null) { trace = new XYChart.Series<>(); series.add(trace); }
                LogSample sample = entry.get();
                trace.getData().add(new XYChart.Data<>((sample.timeMs() - origin) / 1000.0, selected.value(sample)));
            }
            chart.setTitle("Últimas amostras USB · " + selected);
        }
        chart.getData().setAll(series);
    }

    private void advancePlayback() {
        long now = System.nanoTime();
        playbackTime += (now - playbackTick) / 1_000_000.0 * speed.getValue(); playbackTick = now;
        long origin = offline.get(0).timeMs();
        int low = index(), high = offline.size() - 1;
        while (low < high) {
            int middle = low + (high - low + 1) / 2;
            if (offline.get(middle).timeMs() - origin <= playbackTime) low = middle;
            else high = middle - 1;
        }
        cursor.setValue(low);
        if (low == offline.size() - 1) stopPlayback();
    }

    public void stopPlayback() { playback.stop(); play.setText("Reproduzir"); }
}
