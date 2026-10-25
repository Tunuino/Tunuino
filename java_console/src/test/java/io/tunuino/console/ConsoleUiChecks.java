/* Copyright (c) 2026 Tunuino. */
package io.tunuino.console;

import io.tunuino.calibration.CalibrationMap;
import io.tunuino.datalog.LogChannel;
import io.tunuino.datalog.LogSample;
import java.awt.image.BufferedImage;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import javax.imageio.ImageIO;
import javafx.application.Application;
import javafx.application.Platform;
import javafx.animation.PauseTransition;
import javafx.util.Duration;
import javafx.scene.Scene;
import javafx.scene.control.Button;
import javafx.scene.control.ComboBox;
import javafx.scene.control.Label;
import javafx.scene.control.Slider;
import javafx.scene.control.ToggleButton;
import javafx.scene.image.WritableImage;
import javafx.stage.Stage;
import javafx.stage.Window;

/** Exercises the actual application layout with no USB discovery or programmer process. */
public final class ConsoleUiChecks {
    private ConsoleUiChecks() {}
    private static void check(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }

    public static void main(String[] args) { Application.launch(InterfaceCheck.class, args); }

    public static final class InterfaceCheck extends Application {
        private TunuinoConsole console;
        private Path snapshots;
        @FunctionalInterface private interface Step { void run() throws Exception; }

        private void later(Step step) {
            PauseTransition pause = new PauseTransition(Duration.millis(250));
            pause.setOnFinished(event -> {
                try { step.run(); }
                catch (Throwable error) { error.printStackTrace(); if (console != null) console.stop(); System.exit(1); }
            });
            pause.play();
        }

        private void snapshot(Scene scene, String name) throws Exception {
            scene.getRoot().applyCss(); scene.getRoot().layout();
            if (snapshots == null) return;
            WritableImage image = scene.snapshot(null);
            BufferedImage png = new BufferedImage((int) image.getWidth(), (int) image.getHeight(), BufferedImage.TYPE_INT_ARGB);
            for (int y = 0; y < png.getHeight(); ++y) for (int x = 0; x < png.getWidth(); ++x) {
                png.setRGB(x, y, image.getPixelReader().getArgb(x, y));
            }
            ImageIO.write(png, "png", snapshots.resolve(name + ".png").toFile());
        }

        @Override public void start(Stage stage) throws Exception {
            String destination = getParameters().getNamed().get("snapshots");
            if (destination != null) { snapshots = Path.of(destination); Files.createDirectories(snapshots); }
            console = new TunuinoConsole(); console.start(stage, true);
            later(() -> checkDashboard(stage));
        }

        private void checkDashboard(Stage stage) throws Exception {
            Scene scene = stage.getScene();
            snapshot(scene, "console-offline");
            check(scene.getStylesheets().size() == 1, "Shared theme loaded");
            check(((Button) scene.lookup("#monitor-live")).isDisabled(), "No invented sensor capability");
            TelemetryPane telemetry = (TelemetryPane) scene.lookup(".telemetry-pane");
            check(((Label) scene.lookup("#telemetry-value-rpm")).getText().equals("—"), "Offline values are absent");
            List<LogSample> samples = new ArrayList<>();
            for (int i = 0; i < 900; ++i) {
                double rpm = 950 + 2300 * (1 - Math.cos(i / 95.0)) / 2;
                samples.add(new LogSample(i * 100L, rpm, 40 + 48 * (rpm - 950) / 2300,
                    1.0 + 0.04 * Math.sin(i / 27.0), 82 + i / 180.0));
            }
            telemetry.showOffline("simulated-interface.csv · DEMONSTRAÇÃO", samples);
            Slider cursor = (Slider) scene.lookup("#telemetry-cursor"); cursor.setValue(450);
            check(((Label) scene.lookup("#telemetry-value-rpm")).getText().equals(LogChannel.RPM.format(samples.get(450))), "Cursor chooses exact sample");
            ((Button) scene.lookup("#telemetry-next")).fire();
            check(cursor.getValue() == 451, "Next sample navigation");
            @SuppressWarnings("unchecked") ComboBox<LogChannel> channel = (ComboBox<LogChannel>) scene.lookup("#telemetry-channel");
            channel.setValue(LogChannel.LAMBDA); channel.setValue(LogChannel.RPM);
            snapshot(scene, "console-datalog");
            ((Button) scene.lookup("#telemetry-play")).fire();
            check(((Button) scene.lookup("#telemetry-play")).getText().equals("Pausar"), "Playback control starts");
            telemetry.stopPlayback();
            stage.setWidth(1060); stage.setHeight(720);
            later(() -> {
                checkCompact(stage, "console-compact");
                stage.setWidth(960); stage.setHeight(680);
                later(() -> {
                    checkCompact(stage, "console-small");
                    stage.setWidth(1340); stage.setHeight(880);
                    later(() -> checkWorkspaces(stage, telemetry, samples));
                });
            });
        }

        private void checkCompact(Stage stage, String name) throws Exception {
            Scene scene = stage.getScene();
            snapshot(scene, name);
            check(scene.getWidth() <= stage.getWidth() && scene.getHeight() <= stage.getHeight(), "Resize actually completed");
            for (String id : List.of("#telemetry-next", "#console-status", "#telemetry-value-coolant")) {
                javafx.scene.Node node = scene.lookup(id);
                var bounds = node.localToScene(node.getBoundsInLocal());
                check(bounds.getMaxY() < scene.getHeight() && bounds.getMaxX() < scene.getWidth(), "Control stays within compact window: " + id);
            }
            var rpm = scene.lookup("#telemetry-value-rpm"); var coolant = scene.lookup("#telemetry-value-coolant");
            check(Math.abs(rpm.localToScene(rpm.getBoundsInLocal()).getMinY()
                - coolant.localToScene(coolant.getBoundsInLocal()).getMinY()) < 1, "Four indicators fit on one row");
        }

        private void checkWorkspaces(Stage stage, TelemetryPane telemetry, List<LogSample> samples) throws Exception {
            Scene scene = stage.getScene();
            ((ToggleButton) scene.lookup("#navigate-1")).fire(); snapshot(scene, "console-maps");
            check(scene.lookup("#open-map") != null, "Maps workspace is reachable");
            ((ToggleButton) scene.lookup("#navigate-2")).fire(); snapshot(scene, "console-firmware");
            ((ToggleButton) scene.lookup("#navigate-0")).fire();
            telemetry.beginLive(); telemetry.showLive(Optional.of(samples.get(0))); telemetry.showLive(Optional.empty());
            check(((Label) scene.lookup("#telemetry-value-rpm")).getText().equals("—"), "Unavailable data clears current values");
            telemetry.endLive("Monitorização terminada");
            int[] speeds = new int[12], loads = new int[12]; int[][] cells = new int[12][12];
            for (int i = 0; i < 12; ++i) {
                speeds[i] = 800 + 500 * i; loads[i] = 20000 + 10000 * i;
                for (int j = 0; j < 12; ++j) cells[i][j] = 30000 + 3000 * i + 2000 * j;
            }
            CalibrationMap map = new CalibrationMap(CalibrationMap.Kind.VE, speeds, loads, cells);
            CalibrationViewer.show(stage, "simulated-interface.tnmap · DEMONSTRAÇÃO", map);
            for (Window window : List.copyOf(Window.getWindows())) {
                if (window != stage && window instanceof Stage editor && editor.isShowing()) {
                    snapshot(editor.getScene(), "console-map-editor");
                    var colors = (javafx.scene.control.CheckBox) editor.getScene().lookup("#map-colors");
                    check(colors.isSelected(), "Value shading enabled"); colors.fire(); check(!colors.isSelected(), "Value shading can be disabled");
                }
            }
            check(CalibrationViewer.closeAll(), "Unchanged map editor closes cleanly");
            console.stop(); stage.close();
            System.out.println("Console UI: actual shell, shared theme, all workspaces, channel selection, cursor, playback, missing data, compact layouts and map editor passed. Synthetic data; no USB devices.");
            Platform.exit();
        }
    }
}
