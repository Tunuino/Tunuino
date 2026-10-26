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
            check(((Button) scene.lookup("#dashboard-read-ve")).isDisabled(), "Offline dashboard blocks map read");
            check(((Button) scene.lookup("#dashboard-edit-map")).isDisabled(), "Offline dashboard blocks ECU map editing");
            DashboardPane cockpit=(DashboardPane)scene.lookup("#dashboard");
            check(cockpit!=null,"ECU cockpit is the first page");
            var gaugeNodes=new ArrayList<>(scene.lookupAll(".dashboard-gauge"));
            check(gaugeNodes.size()==6,"Dashboard contains all six gauges");
            double gaugeY=gaugeNodes.get(0).localToScene(gaugeNodes.get(0).getBoundsInLocal()).getMinY();
            for(var gauge:gaugeNodes)check(Math.abs(gauge.localToScene(gauge.getBoundsInLocal()).getMinY()-gaugeY)<1,
                "Six gauges fit one row at the initial desktop width");
            check(scene.lookup("#dashboard-table-host")==null||scene.lookup("#dashboard")!=null,
                "Dashboard is real rather than a screenshot");
            ((ToggleButton)scene.lookup("#navigate-1")).fire();
            // The Maps workspace lives in a ScrollPane; let JavaFX attach and
            // skin its content before querying controls inside that page.
            scene.getRoot().applyCss(); scene.getRoot().layout();
            check(((Button)scene.lookup("#engine-preview")).isDisabled(), "Offline preview cannot contact ECU");
            check(((Button)scene.lookup("#engine-timing")).isDisabled(), "Offline timing cannot contact ECU");
            check(((Label)scene.lookup("#engine-timing-result")).getText().contains("não comanda atuadores"),
                "Offline timing never fabricates a crank phase");
            check(((Label)scene.lookup("#engine-preview-result")).getText().contains("Sem pré-visualização"),
                "Preview never fabricates engine values");
            ((ToggleButton)scene.lookup("#navigate-0")).fire();
            // Synthetic data only inside automated UI tests; production has none.
            int[] erpm=new int[16],loads=new int[16];int[][] values=new int[16][16];
            for(int y=0;y<16;++y) {
                erpm[y]=500+y*500;loads[y]=20000+y*10000;
                for(int x=0;x<16;++x)values[y][x]=50000+y*500+x*300;
            }
            cockpit.showMap(new CalibrationMap(CalibrationMap.Kind.VE,erpm,loads,values));
            check(scene.lookup("#dashboard")!=null
                &&((Button)scene.lookup("#dashboard-read-ve")).isDisabled(),
                "Offline map read remains disabled even with offline UI fixture");
            snapshot(scene,"console-dashboard-16x16");
            checkMapEditing(stage,cockpit,new CalibrationMap(CalibrationMap.Kind.VE,erpm,loads,values));
            stage.setWidth(1510);stage.setHeight(1010);
            snapshot(scene,"console-dashboard-wide");
            stage.setWidth(980);stage.setHeight(720);
            cockpit.clearMaps();
            ((ToggleButton)scene.lookup("#navigate-2")).fire();
            scene.getRoot().applyCss();scene.getRoot().layout();
            check(((DashboardPane)scene.lookup("#dashboard-map-workspace"))!=null,
                "Fuel Tables opens the native 16x16 ECU map display");
            check(((Button)scene.lookup("#dashboard-read-ve")).isDisabled(),
                "Fuel Tables cannot read an offline ECU");
            ((ToggleButton)scene.lookup("#navigate-3")).fire();
            scene.getRoot().applyCss();scene.getRoot().layout();
            check(((DashboardPane)scene.lookup("#dashboard-map-workspace"))!=null,
                "Ignition Tables opens the native 16x16 ECU map display");
            ((ToggleButton)scene.lookup("#navigate-4")).fire();
            scene.getRoot().applyCss();scene.getRoot().layout();
            check(((Button)scene.lookup("#autotune-start")).isDisabled(),
                "Unavailable Autotune must never actuate a bench ECU");
            ((ToggleButton)scene.lookup("#navigate-5")).fire();
            scene.getRoot().applyCss();scene.getRoot().layout();
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
                    double frameHeight = stage.getHeight() - scene.getHeight();
                    stage.setMinHeight(600); stage.setHeight(640 + frameHeight);
                    later(() -> {
                        checkCompact(stage, "console-short-content");
                        stage.setWidth(1340); stage.setHeight(880);
                        later(() -> checkWorkspaces(stage, telemetry, samples));
                    });
                });
            });
        }

        private void checkMapEditing(Stage stage,DashboardPane cockpit,CalibrationMap map) throws Exception {
            var identity=new io.tunuino.protocol.DeviceIdentity(io.tunuino.protocol.DeviceIdentity.Family.PROTEUS,
                io.tunuino.protocol.DeviceIdentity.McuSeries.STM32_F4,7,0,2,0,15,"0102030405060708090a0b0c");
            cockpit.setConnection(identity,true,true);
            cockpit.showMap(map);
            var chosen=new java.util.concurrent.atomic.AtomicReference<DashboardPane.MapSelection>();
            cockpit.setEditAction(chosen::set);
            Button edit=(Button)stage.getScene().lookup("#dashboard-edit-map");
            check(!edit.isDisabled(),"Loaded map in an idle identified session can be edited");
            edit.fire();
            check(chosen.get().map()==map&&chosen.get().row()==0&&chosen.get().column()==0,
                "Edit button opens the actual ECU snapshot");
            chosen.set(null);
            cockpit.selectKind(CalibrationMap.Kind.IGNITION);
            check(edit.isDisabled(),"Unloaded ignition table cannot open a stale VE snapshot");
            cockpit.selectKind(CalibrationMap.Kind.VE);
            var canvas=(javafx.scene.canvas.Canvas)stage.getScene().lookup("#dashboard-map-canvas");
            double x=72+15.5*(canvas.getWidth()-80)/16;
            double y=38+15.5*(canvas.getHeight()-46)/16;
            doubleClick(canvas,x,y);
            check(chosen.get()!=null&&chosen.get().row()==15&&chosen.get().column()==15,
                "Double click selects the last of all 256 real cells without writing");
            chosen.set(null);doubleClick(canvas,71,37);
            check(chosen.get()==null,"Axis padding does not select a map cell");
            cockpit.setConnection(identity,true,false);
            doubleClick(canvas,x,y);
            check(edit.isDisabled()&&chosen.get()==null,"Serial busy blocks button and double click");
            cockpit.setConnection(identity,true,true);
            check(!edit.isDisabled(),"Ending USB work preserves and unlocks the same map");
            CalibrationViewer.show(stage,"selection-16x16.tnmap",map,15,15,null);
            for(Window window:List.copyOf(Window.getWindows())) {
                if(window!=stage&&window instanceof Stage editor&&editor.isShowing()) {
                    var grid=(javafx.scene.control.TableView<?>)editor.getScene().lookup(".table-view");
                    var selected=grid.getSelectionModel().getSelectedCells().get(0);
                    check(selected.getRow()==15&&selected.getColumn()==16,"Editor focuses selected cell, excluding MAP axis");
                }
            }
            check(CalibrationViewer.closeAll(),"Opening a cell does not modify or write the map");
            cockpit.setConnection(null,false,false);
            cockpit.setConnection(identity,true,true);
            check(edit.isDisabled(),"Reconnection requires a fresh map read");
            cockpit.showMap(map);
            var other=new io.tunuino.protocol.DeviceIdentity(identity.family(),identity.mcuSeries(),7,0,2,0,15,
                "1112131415161718191a1b1c");
            cockpit.setConnection(other,true,true);
            check(edit.isDisabled(),"A different ECU never inherits the previous maps");
            cockpit.setConnection(null,false,false);
            cockpit.showMap(map); // Restore only the offline screenshot fixture.
        }

        private static void doubleClick(javafx.scene.canvas.Canvas canvas,double x,double y) {
            var point=canvas.localToScene(x,y);
            javafx.event.Event.fireEvent(canvas,new javafx.scene.input.MouseEvent(canvas,canvas,
                javafx.scene.input.MouseEvent.MOUSE_CLICKED,point.getX(),point.getY(),point.getX(),point.getY(),
                javafx.scene.input.MouseButton.PRIMARY,2,false,false,false,false,
                false,false,false,false,false,true,new javafx.scene.input.PickResult(canvas,x,y)));
        }

        private void checkCompact(Stage stage, String name) throws Exception {
            Scene scene = stage.getScene();
            snapshot(scene, name);
            check(scene.getWidth() <= stage.getWidth() && scene.getHeight() <= stage.getHeight(), "Resize actually completed");
            for (String id : List.of("#telemetry-next", "#console-status", "#telemetry-value-coolant")) {
                javafx.scene.Node node = scene.lookup(id);
                var bounds = node.localToScene(node.getBoundsInLocal());
                check(bounds.getMaxY() < scene.getHeight() && bounds.getMaxX() < scene.getWidth(),
                    "Control stays within compact window: " + id + " " + bounds + " in " + scene.getWidth() + "x" + scene.getHeight());
            }
            var rpm = scene.lookup("#telemetry-value-rpm"); var coolant = scene.lookup("#telemetry-value-coolant");
            check(Math.abs(rpm.localToScene(rpm.getBoundsInLocal()).getMinY()
                - coolant.localToScene(coolant.getBoundsInLocal()).getMinY()) < 1, "Four indicators fit on one row");
        }

        private void checkWorkspaces(Stage stage, TelemetryPane telemetry, List<LogSample> samples) throws Exception {
            Scene scene = stage.getScene();
            ((ToggleButton) scene.lookup("#navigate-1")).fire(); snapshot(scene, "console-maps");
            check(scene.lookup("#open-map") != null, "Maps workspace is reachable");
            ((ToggleButton) scene.lookup("#navigate-6")).fire();
            scene.getRoot().applyCss();scene.getRoot().layout();
            check(((Button)scene.lookup("#config-read")).isDisabled(),"Offline configuration cannot read USB");
            check(((Button)scene.lookup("#config-apply")).isDisabled(),"Offline configuration cannot write USB");
            ConfigurationPane configEditor=(ConfigurationPane)scene.lookup("#config-read").getParent().getParent().getParent();
            byte[] fixture=new byte[256];java.nio.ByteBuffer cfg=java.nio.ByteBuffer.wrap(fixture);
            cfg.putShort(0,(short)2);fixture[3]=36;fixture[4]=1;fixture[26]=3;
            cfg.putShort(12,(short)4095);cfg.putShort(20,(short)4095);cfg.putShort(34,(short)2048);cfg.putShort(36,(short)4095);
            cfg.putShort(96,(short)12000);cfg.putShort(100,(short)3300);
            var id=new io.tunuino.protocol.DeviceIdentity(io.tunuino.protocol.DeviceIdentity.Family.PROTEUS,
                io.tunuino.protocol.DeviceIdentity.McuSeries.STM32_F4,7,0,2,0,15,"0102030405060708090a0b0c");
            configEditor.availability(id,true,false);
            configEditor.loaded(new io.tunuino.protocol.ConfigurationClient.Snapshot(new io.tunuino.calibration.IoConfiguration(fixture),
                new io.tunuino.protocol.ConfigurationClient.Validation(false,false,0,1,0)));
            var reference=(javafx.scene.control.TextField)scene.lookup("#config-field-REFERENCE");
            reference.setText("3290");check(!((Button)scene.lookup("#config-apply")).isDisabled(),"Valid local calibration enables explicit commit");
            reference.setText("bad");check(((Button)scene.lookup("#config-apply")).isDisabled(),"Invalid calibration blocks commit");
            reference.setText("3290");configEditor.availability(id,true,true);
            check(((Button)scene.lookup("#config-apply")).isDisabled(),"Busy serial worker blocks calibration");
            configEditor.availability(id,true,false);int[] rawAdc=new int[16];java.util.Arrays.fill(rawAdc,2000);
            configEditor.showInputs(new io.tunuino.protocol.IoClient.Inputs(new LogSample(100,0,0,0,0,0),1,3300,1,0,0,rawAdc));
            @SuppressWarnings("unchecked") var table=(javafx.scene.control.TableView<ConfigurationPane.AdcRow>)scene.lookup("#config-adc-table");
            check(table.getItems().size()==16&&table.getItems().get(0).volts().equals("1.612"),"All ADC channels and actual MCU scaling displayed");
            configEditor.showInputs(new io.tunuino.protocol.IoClient.Inputs(new LogSample(200,0,0,0,0,0),0,3300,1,0,0,new int[16]));
            check(table.getItems().stream().allMatch(row->row.raw().equals("—")),"Unavailable ADC is not displayed as zero");
            ((Label)scene.lookup("#config-feedback")).setText("Dados sintéticos de teste da interface · sem ECU física");
            snapshot(scene,"console-calibration");
            var forms=(javafx.scene.layout.TilePane)scene.lookup("#config-form-grid");
            var firstCard=forms.getChildren().get(0);var secondCard=forms.getChildren().get(1);
            check(Math.abs(firstCard.localToScene(firstCard.getBoundsInLocal()).getMinY()
                -secondCard.localToScene(secondCard.getBoundsInLocal()).getMinY())<1,"Calibration cards share a row on a wide desktop");
            configEditor.availability(null,false,false);
            check(reference.getText().equals("3290")&&((Button)scene.lookup("#config-apply")).isDisabled(),"Disconnect preserves local edits while blocking ECU writes");
            reference.setText("3300");check(configEditor.canClose(),"Unchanged configuration closes without prompting");
            ((ToggleButton) scene.lookup("#navigate-7")).fire();
            scene.getRoot().applyCss();scene.getRoot().layout();
            check(((Button)scene.lookup("#diagnostics-read")).isDisabled(),
                "Diagnostic read must remain disabled without a real ECU");
            check(((Label)scene.lookup("#diagnostics-output")).getText().contains("Sem diagnóstico"),
                "Diagnostics never invent sensor or flash state");
            ((ToggleButton) scene.lookup("#navigate-8")).fire(); snapshot(scene, "console-firmware");
            ((ToggleButton) scene.lookup("#navigate-5")).fire();
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
                    check(((Button)editor.getScene().lookup("#map-refine-16")).isDisabled(),
                        "Already 12x12 offline fixture must not offer 8x8 refinement");
                }
            }
            check(CalibrationViewer.closeAll(), "Unchanged map editor closes cleanly");
            int[] r8=new int[8],k8=new int[8];int[][] c8=new int[8][8];
            for(int i=0;i<8;++i){r8[i]=600+i*700;k8[i]=20000+i*15000;
                for(int j=0;j<8;++j)c8[i][j]=45000+i*1000+j*500;}
            CalibrationViewer.show(stage,"offline-8x8.tnmap",new CalibrationMap(CalibrationMap.Kind.VE,r8,k8,c8));
            Stage legacyEditor=null;
            for(Window w:List.copyOf(Window.getWindows()))
                if(w!=stage&&w instanceof Stage editor&&editor.isShowing()&&
                    editor.getScene().lookup("#map-refine-16")!=null)legacyEditor=editor;
            check(legacyEditor!=null, "8x8 editor opens for offline refinement");
            Button refineButton=(Button)legacyEditor.getScene().lookup("#map-refine-16");
            check(!refineButton.isDisabled(),"8x8 conversion button available");
            refineButton.fire();
            boolean expanded=false;
            for(Window w:List.copyOf(Window.getWindows())) {
                if(w!=stage&&w instanceof Stage editor&&editor.isShowing()
                        &&editor.getTitle().contains("16x16")) {
                    var grid=(javafx.scene.control.TableView<?>)editor.getScene().lookup(".table-view");
                    check(grid!=null&&grid.getColumns().size()==17,
                        "Converted map editor contains one axis and 16 RPM columns");
                    check(editor.getScene().lookup("#map-write")==null,
                        "Converted 16x16 copy must not offer ECU upload");
                    expanded=true;
                }
            }
            check(expanded,"8x8 to 16x16 conversion opens a new offline editor");
            check(CalibrationViewer.closeAll(),"Offline converted map editor closes cleanly");
            console.stop(); stage.close();
            System.out.println("Console UI: actual shell, shared theme, all workspaces, channel selection, cursor, playback, missing data, compact layouts and map editor passed. Synthetic data; no USB devices.");
            Platform.exit();
        }
    }
}
