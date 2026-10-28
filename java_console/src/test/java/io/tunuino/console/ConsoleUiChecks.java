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
            check(((Label)scene.lookup("#console-active-build")).getText().contains(io.tunuino.maintenance.BuildInfo.label()),
                "Active source revision is visible beside the product name");
            check(scene.getRoot().lookupAll(".navigation-group").size()==1,"Flat reference navigation with one secondary motor group");
            check(((Button) scene.lookup("#dashboard-read-ve")).isDisabled(), "Offline dashboard blocks map read");
            check(((Button) scene.lookup("#dashboard-edit-map")).isDisabled(), "Offline dashboard blocks ECU map editing");
            DashboardPane cockpit=(DashboardPane)scene.lookup("#dashboard");
            check(cockpit!=null,"ECU cockpit is the first page");
            var gaugeNodes=new ArrayList<>(scene.getRoot().lookupAll(".dashboard-gauge"));
            check(gaugeNodes.size()==6,"Dashboard contains all six gauges");
            checkCanvasSizing(scene);
            var logo=(javafx.scene.image.ImageView)scene.lookup("#console-logo");
            check(logo.isPreserveRatio()&&logo.getImage().getUrl().endsWith("/tunuino.png"),
                "Header uses the original logo resource with its proportions preserved");

            var gaugeRows=new java.util.HashSet<Long>();
            for(var gauge:gaugeNodes)gaugeRows.add(Math.round(gauge.localToScene(gauge.getBoundsInLocal()).getMinY()));
            boolean wide=((javafx.scene.layout.FlowPane)gaugeNodes.get(0).getParent()).getWidth()>=760;
            check(gaugeRows.size()==(wide?1:2),"Gauges fit one desktop row or two compact rows");
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
            cockpit.showMap(io.tunuino.calibration.BaseMaps.create(io.tunuino.calibration.BaseMaps.Template.NATURALLY_ASPIRATED,CalibrationMap.Kind.VE));
            cockpit.showMap(io.tunuino.calibration.BaseMaps.create(io.tunuino.calibration.BaseMaps.Template.NATURALLY_ASPIRATED,CalibrationMap.Kind.IGNITION));
            cockpit.selectKind(CalibrationMap.Kind.VE);
            cockpit.showSample(Optional.of(new LogSample(1000,3250,62,1,92)));
            cockpit.showAuxiliary(new io.tunuino.protocol.AuxControlClient.FeatureState(0,7,3700,3800,1380,0,1000,10000,0,0,1000,3000,0,0,1));
            byte[] statusProfile=new byte[128];statusProfile[0]=1;statusProfile[22]=5;statusProfile[23]=6;statusProfile[24]=7;
            cockpit.showOutputs(new io.tunuino.protocol.EngineOutputClient.State(0,0,1L<<4,0,true,4,true,1,0,true,false),statusProfile);
            check(((Label)scene.lookup("#dashboard-output-pump")).getText().contains("ON")&&((Label)scene.lookup("#dashboard-output-fan")).getText().contains("OFF"),"Dashboard uses the actual configured relay channels, not fixed contacts");
            snapshot(scene,"console-reference-layout");
            stage.setWidth(1510);stage.setHeight(1010);
            later(()->checkWideDashboard(stage));
        }
        private void checkCanvasSizing(Scene scene) {
            var canvas=(javafx.scene.canvas.Canvas)scene.lookup("#dashboard-map-canvas");
            var host=(javafx.scene.layout.Region)canvas.getParent();
            double preferred=host.prefWidth(-1);
            host.resize(900,280);host.layout();
            check(Math.abs(host.prefWidth(-1)-preferred)<0.1,
                "Canvas rendering must not change its container preferred width");
            host.resize(360,240);host.layout();
            check(canvas.getWidth()<=360&&canvas.getHeight()<=240,
                "Canvas fits a smaller host, including its borders");
            scene.getRoot().requestLayout();scene.getRoot().layout();
        }

        private void checkWideDashboard(Stage stage) throws Exception {
            Scene scene=stage.getScene();DashboardPane cockpit=(DashboardPane)scene.lookup("#dashboard");
            snapshot(scene,"console-reference-layout");snapshot(scene,"console-dashboard-wide");
            var build=(Label)scene.lookup("#console-active-build");
            check(build.getWidth()>=build.prefWidth(-1),"Active build remains readable in the header");
            var center=(javafx.scene.control.SplitPane)scene.lookup("#dashboard-center-split");
            var lower=(javafx.scene.control.SplitPane)scene.lookup("#dashboard-lower-split");
            var tables=(javafx.scene.control.SplitPane)scene.lookup("#dashboard-tables-split");
            double width=((javafx.scene.layout.Region)center.getItems().get(0)).getWidth();
            double height=((javafx.scene.layout.Region)tables.getItems().get(0)).getHeight();
            center.setDividerPositions(.66);lower.setDividerPositions(.50);tables.setDividerPositions(.70);
            scene.getRoot().layout();
            check(((javafx.scene.layout.Region)center.getItems().get(0)).getWidth()<width-40,
                "Map and engine state divider changes the table width");
            check(((javafx.scene.layout.Region)tables.getItems().get(0)).getHeight()>height+10,
                "Table divider changes the allocated height");
            center.setDividerPositions(.80);lower.setDividerPositions(.63);tables.setDividerPositions(.60);
            stage.setWidth(980);stage.setHeight(720);
            later(()->checkCompactDashboard(stage));
        }
        private void checkCompactDashboard(Stage stage) throws Exception {
            Scene scene=stage.getScene();DashboardPane cockpit=(DashboardPane)scene.lookup("#dashboard");
            scene.getRoot().applyCss();scene.getRoot().layout();
            var canvas=(javafx.scene.canvas.Canvas)scene.lookup("#dashboard-map-canvas");
            double width=canvas.getWidth(),height=canvas.getHeight();
            for(int i=0;i<60;i++){scene.getRoot().requestLayout();scene.getRoot().layout();}
            check(Math.abs(canvas.getWidth()-width)<1&&Math.abs(canvas.getHeight()-height)<1,
                "Repeated layouts never stretch the table while the window is unchanged");
            check(canvas.localToScene(canvas.getBoundsInLocal()).getMaxX()<scene.getWidth(),
                "Compact map stays inside the visible window");
            snapshot(scene,"console-dashboard-resized");
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
                double frameWidth=stage.getWidth()-scene.getWidth();
                stage.setMinWidth(940);stage.setWidth(944+frameWidth);stage.setHeight(680);
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
                "Double click selects the last of all 256 real cells without writing: "+chosen.get()+" canvas "+canvas.getWidth()+"x"+canvas.getHeight());
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
                false,false,false,false,false,true,new javafx.scene.input.PickResult(canvas,new javafx.geometry.Point3D(x,y,0),1)));
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

        private void auditRouting(javafx.scene.Node root, String page) throws Exception {
            var seen = java.util.Collections.newSetFromMap(new java.util.IdentityHashMap<javafx.scene.Node, Boolean>());
            var pending = new java.util.ArrayDeque<javafx.scene.Node>(); pending.add(root);
            var rows = new ArrayList<String>();
            while (!pending.isEmpty()) {
                var node = pending.removeFirst(); if (!seen.add(node)) continue;
                if (node instanceof javafx.scene.Parent parent) pending.addAll(parent.getChildrenUnmodifiable());
                if (node instanceof javafx.scene.control.TabPane tabs)
                    for (var tab : tabs.getTabs()) if (tab.getContent() != null) pending.add(tab.getContent());
                if (node instanceof javafx.scene.control.TitledPane pane && pane.getContent() != null) pending.add(pane.getContent());
                if (node instanceof javafx.scene.control.ScrollPane pane && pane.getContent() != null) pending.add(pane.getContent());
                if (node instanceof ComboBox<?> choice && choice.getStyleClass().contains("routing-choice")) {
                    check(!choice.isEditable(), page + ": routing cannot expose a raw numeric editor");
                    check(choice.getItems().size() <= 3, page + ": at most three quick routing choices: " + choice.getId());
                    var labels = new ArrayList<String>();
                    for (Object value : choice.getItems()) {
                        @SuppressWarnings("unchecked") var typed = (ComboBox<Object>) choice;
                        String text = typed.getConverter().toString(value); labels.add(text);
                        check(text.equals("Desativada") || text.matches("(Recomendada|Proposta|Atual) · (Preto|Branco) 35/[0-9]+"),
                            page + ": only connector choices, never repeated function names or MCU GPIO: " + text);
                    }
                    var customize = (Button) choice.getProperties().get("routing-customize");
                    check(customize != null && customize.getParent() == choice.getParent(), page + ": full choices require a separate Customize button");
                    check(new java.util.HashSet<>(labels).size() == labels.size(), page + ": duplicate quick choices");
                    rows.add(choice.getId() + "\t" + String.join(" | ", labels));
                }
            }
            int expected = switch (page) { case "sensors" -> 6; case "tps-idle-boost" -> 2; case "engine" -> 27;
                case "fuel-protections" -> 2; case "io-functions" -> 13; default -> throw new IllegalArgumentException(page); };
            check(rows.size() == expected, page + ": every routing selector was inspected: " + rows.size() + "/" + expected);
            if (snapshots != null) Files.write(snapshots.resolve("routing-audit-" + page + ".tsv"), rows);
            System.out.println("Routing audit " + page + ": " + rows.size() + " selectors passed");
        }

        private <T> void customRoute(ComboBox<T> choice, T custom) {
            T original = choice.getValue();
            choice.setValue(custom);
            check(choice.getValue().equals(custom) && choice.getItems().contains(custom), "Existing custom routing survives in " + choice.getId());
            check(choice.getItems().size() <= 3, "Custom routing keeps the short list in " + choice.getId());
            if (custom instanceof String) check(choice.getEditor().getText().equals(custom), "Wire channel retained exactly in " + choice.getId());
            choice.setValue(original);
        }

        private void checkWorkspaces(Stage stage, TelemetryPane telemetry, List<LogSample> samples) throws Exception {
            Scene scene = stage.getScene();
            ((ToggleButton) scene.lookup("#navigate-1")).fire(); snapshot(scene, "console-maps");
            check(scene.lookup("#open-map") != null, "Maps workspace is reachable");
            ((ToggleButton) scene.lookup("#navigate-6")).fire();
            scene.getRoot().applyCss();scene.getRoot().layout();
            check(((Button)scene.lookup("#config-read")).isDisabled(),"Offline configuration cannot read USB");
            check(((Button)scene.lookup("#config-apply")).isDisabled(),"Offline configuration cannot write USB");
            check(scene.lookup("#engine-control-pane")==null,"Sensors do not contain motor arming or actuator tests");
            ConfigurationPane configEditor=(ConfigurationPane)scene.lookup("#sensor-configuration-pane");
            check(configEditor.candidate().errors().isEmpty(),"Offline draft is available without inventing sensor channels");
            @SuppressWarnings("unchecked") var wheelPreset=(ComboBox<String>)scene.lookup("#config-preset-Aquisição");
            wheelPreset.setValue("60-2");((Button)scene.lookup("#config-preset-apply-Aquisição")).fire();
            check(configEditor.candidate().value(io.tunuino.calibration.IoConfiguration.Field.WHEEL).equals("60")
                &&!configEditor.candidate().triggerEnabled(),"Wheel preset edits local values without enabling capture");
            var sensorTabs=(javafx.scene.control.TabPane)scene.lookup("#config-form-tabs");
            check(sensorTabs.getTabs().size()==8,"All six sensor calibrations and trigger/ADC diagnostics share one workspace");
            sensorTabs.getSelectionModel().select(3);scene.getRoot().applyCss();scene.getRoot().layout();
            var ntc=(javafx.scene.control.TitledPane)scene.lookup("#clt-ntc");ntc.setExpanded(true);scene.getRoot().applyCss();scene.getRoot().layout();
            ((Button)scene.lookup("#clt-ntc-apply")).fire();
            check(configEditor.candidate().coolantChannel()==0&&Double.parseDouble(configEditor.candidate().value(io.tunuino.calibration.IoConfiguration.Field.TEMP_1))==120,
                "NTC assistant prepares a sorted curve without assigning or enabling a sensor");
            check(((Button)scene.lookup("#config-measure-TEMP_RAW_1")).isDisabled(),"Offline temperature capture cannot read invented ADC data");
            snapshot(scene,"console-sensor-ntc");ntc.setExpanded(false);
            for(int tab=4;tab<=6;tab++){sensorTabs.getSelectionModel().select(tab);scene.getRoot().applyCss();scene.getRoot().layout();snapshot(scene,"console-sensor-calibration-"+tab);}
            // Restore the core draft before later close/preservation checks.
            for(var f:java.util.List.of(io.tunuino.calibration.IoConfiguration.Field.TEMP_RAW_1,io.tunuino.calibration.IoConfiguration.Field.TEMP_RAW_2,io.tunuino.calibration.IoConfiguration.Field.TEMP_RAW_3,io.tunuino.calibration.IoConfiguration.Field.TEMP_1,io.tunuino.calibration.IoConfiguration.Field.TEMP_2,io.tunuino.calibration.IoConfiguration.Field.TEMP_3)){
                var field=(javafx.scene.control.TextField)sensorTabs.getTabs().get(3).getContent().lookup("#config-field-"+f.name());field.setText(io.tunuino.calibration.IoConfiguration.localDefaults().value(f));
            }
            auditRouting(configEditor,"sensors");
            for (String key : List.of("map","lambda","coolant")) {
                @SuppressWarnings("unchecked") var route = (ComboBox<Integer>)configEditor.lookup("#config-" + key + "-channel");
                if (route == null) { // Non-selected tab contents are deliberately not attached to the scene.
                    var tabs=(javafx.scene.control.TabPane)scene.lookup("#config-form-tabs");
                    tabs.getSelectionModel().select(key.equals("map")?1:key.equals("lambda")?2:3);
                    scene.getRoot().applyCss(); scene.getRoot().layout();
                    route=(ComboBox<Integer>)scene.lookup("#config-" + key + "-channel");
                }
                customRoute(route, key.equals("coolant")?4:11);
            }
            ((javafx.scene.control.TabPane)scene.lookup("#config-form-tabs")).getSelectionModel().selectFirst();
            snapshot(scene,"console-sensor-presets");
            ((ToggleButton)scene.lookup("#navigate-10")).fire();scene.getRoot().applyCss();scene.getRoot().layout();
            for(String control:List.of("#engine-arm","#engine-configure","#engine-outputs-read"))
                check(((Button)scene.lookup(control)).isDisabled(),"Offline actuator operation is blocked: "+control);
            check(scene.lookup("#output-test-pane")==null,"Motor configuration does not contain actuator-test controls");
            EngineControlPane enginePane=(EngineControlPane)scene.lookup("#engine-control-pane");
            try{enginePane.profile();throw new AssertionError("An unconfigured motor must not have an invented profile");}
            catch(IllegalArgumentException expected){}
            ((Button)scene.lookup("#engine-preset-apply")).fire();
            @SuppressWarnings("unchecked") ComboBox<Integer> engineCylinders=(ComboBox<Integer>)scene.lookup("#engine-cylinders");
            check(engineCylinders.getValue()==4,"Four-cylinder structural template applied offline");
            snapshot(scene,"console-engine-presets");
            var engineTabs=(javafx.scene.control.TabPane)scene.lookup("#engine-configuration-tabs");
            engineTabs.getSelectionModel().select(1);scene.getRoot().applyCss();scene.getRoot().layout();
            check(((javafx.scene.control.TextField)scene.lookup("#engine-cylinder-1-2")).getText().equals("540"),"1-3-4-2 gives cylinder 2 the correct 720-degree phase");
            auditRouting(enginePane,"engine");
            for (int cylinder=0;cylinder<12;cylinder++) for(int output=0;output<2;output++) {
                @SuppressWarnings("unchecked") var route=(ComboBox<String>)scene.lookup("#engine-cylinder-channel-"+cylinder+"-"+output);
                customRoute(route,output==0?"16":"12");
            }
            snapshot(scene,"console-cylinder-assignments");
            engineCylinders.setValue(1);
            String[][] engineFields={{"fuel","3"},{"dwell","2"},{"dead","0,5"},{"maximum","15"},
                {"injectionEnd","0"},{"toothZero","0"},{"camFirst","1"},{"camLast","2"},{"camPhase","0"}};
            for(String[] field:engineFields)enginePane.setting(field[0],field[1]);
            for(int column=0;column<2;++column){@SuppressWarnings("unchecked") var route=(ComboBox<String>)scene.lookup("#engine-cylinder-channel-0-"+column);route.setValue("1");check(route.getConverter().toString("1").contains(column==0?"Preto 35/3":"Preto 35/35"),"Cylinder choices use function names");}
            ((javafx.scene.control.TextField)scene.lookup("#engine-cylinder-0-2")).setText("180");
            byte[] engineBytes=enginePane.profile();
            check(engineBytes.length==128&&java.nio.ByteBuffer.wrap(engineBytes).getInt(34)==18000,"Explicit cylinder/PMS form produces exact wire units");
            var units=java.nio.ByteBuffer.wrap(engineBytes);
            check(units.getShort(2)==3000&&units.getShort(4)==2000&&units.getShort(6)==500&&units.getShort(8)==15000,
                "Milliseconds and comma decimals preserve exact internal timer units");
            enginePane.setting("dwell","5.001");
            try{enginePane.profile();throw new AssertionError("Unsafe dwell must not leave the editor");}catch(IllegalArgumentException expected){}
            enginePane.setting("dwell","2");
            engineTabs.getSelectionModel().selectFirst();scene.getRoot().applyCss();scene.getRoot().layout();
            ((Button)scene.lookup("#engine-generic-base")).fire();
            check(engineCylinders.getValue()==4,"Generic configuration uses selected firing-order preset");
            engineTabs.getSelectionModel().select(5);scene.getRoot().applyCss();scene.getRoot().layout();
            @SuppressWarnings("unchecked") var pumpRoute=(ComboBox<String>)scene.lookup("#engine-channel-pump");
            check(pumpRoute.getItems().size()==2&&pumpRoute.getValue().equals("10")&&pumpRoute.getConverter().toString("10").contains("Preto 35/20"),"Pump offers disabled and the conventional connector contact, not sixteen repeated choices");
            pumpRoute.setValue("15");check(pumpRoute.getItems().size()==3&&pumpRoute.getValue().equals("15"),"Custom loaded route remains selectable and is not silently replaced");pumpRoute.setValue("10");
            for (String key : List.of("pump","fan","relay")) {
                @SuppressWarnings("unchecked") var route=(ComboBox<String>)scene.lookup("#engine-channel-"+key);
                customRoute(route,"15");
            }
            snapshot(scene,"console-routing-auxiliary-quick");engineTabs.getSelectionModel().selectFirst();scene.getRoot().applyCss();scene.getRoot().layout();
            check(!((javafx.scene.control.CheckBox)scene.lookup("#engine-autonomous")).isSelected(),"Generic base does not enable autonomous startup");
            check(((Button)scene.lookup("#engine-configure")).isDisabled(),"Offline base cannot send a profile");
            snapshot(scene,"console-generic-base");
            engineTabs.getSelectionModel().select(6);scene.getRoot().applyCss();scene.getRoot().layout();
            FuelCorrectionPane fuelPane=(FuelCorrectionPane)scene.lookup("#fuel-correction-pane");
            var correctionsEnabled=(javafx.scene.control.CheckBox)scene.lookup("#fuel-corrections-enabled");
            check(!correctionsEnabled.isSelected()&&!fuelPane.candidate().configured(),"Existing profiles keep corrections disabled");
            correctionsEnabled.fire();
            check(fuelPane.candidate().enabled()&&fuelPane.candidate().value(7,false)==10000,"Start/warm-up reference is editable and uses ms");
            var crankFuel=(javafx.scene.control.TextField)scene.lookup("#fuel-correction-cranking");crankFuel.setText("150,25");
            check(fuelPane.candidate().value(3,false)==15025,"Exact comma decimal fuel percentage");
            var aseTime=(javafx.scene.control.TextField)scene.lookup("#fuel-correction-duration");aseTime.setText("1000.5");
            try{fuelPane.candidate();throw new AssertionError("Fractional ms duration rounded silently");}catch(IllegalArgumentException expected){}
            aseTime.setText("10000");snapshot(scene,"console-start-warmup");
            correctionsEnabled.fire();check(!fuelPane.candidate().enabled()&&fuelPane.candidate().configured(),"Disabling retains valid values");
            ((ToggleButton)scene.lookup("#navigate-12")).fire();scene.getRoot().applyCss();scene.getRoot().layout();
            AuxControlPane auxPane=(AuxControlPane)scene.lookup("#aux-control-pane");
            check(((Button)scene.lookup("#aux-read")).isDisabled()&&((Button)scene.lookup("#aux-save")).isDisabled(),"Offline auxiliary writes are blocked");
            ((Button)scene.lookup("#aux-reference")).fire();check(auxPane.candidate().idleMode()==0&&auxPane.candidate().boostMode()==0,"Auxiliary reference leaves valves disabled");
            var auxTabs=(javafx.scene.control.TabPane)scene.lookup("#aux-tabs");check(auxTabs.getTabs().size()==2,"TPS calibration is centralized; this page contains idle and boost");
            auditRouting(auxPane,"tps-idle-boost");
            for(int tab=0;tab<2;tab++){auxTabs.getSelectionModel().select(tab);scene.getRoot().applyCss();scene.getRoot().layout();snapshot(scene,"console-idle-boost-"+tab);}
            ((ToggleButton)scene.lookup("#navigate-13")).fire();scene.getRoot().applyCss();scene.getRoot().layout();
            var featuresPane=(FeatureControlPane)scene.lookup("#feature-control-pane");
            check(((Button)scene.lookup("#features-read")).isDisabled()&&((Button)scene.lookup("#features-save")).isDisabled(),"Offline feature writes blocked");
            ((Button)scene.lookup("#features-reference")).fire();check(new io.tunuino.calibration.FeatureConfiguration(featuresPane.candidate().bytes()).flags()==0,"Reference disables optional strategies");
            var featuresTabs=(javafx.scene.control.TabPane)scene.lookup("#features-tabs");check(featuresTabs.getTabs().size()==7,"Dedicated sensor, voltage, lambda, transient, limit, flex and cylinder groups");
            featuresTabs.getSelectionModel().select(1);scene.getRoot().applyCss();scene.getRoot().layout();
            var dead=(javafx.scene.control.TextField)scene.lookup("#feature-field-DEAD1");dead.setText("1,234");check(new io.tunuino.calibration.FeatureConfiguration(featuresPane.candidate().bytes()).raw(io.tunuino.calibration.FeatureConfiguration.Field.DEAD1)==1234,"GUI latency stored in exact ms");
            auditRouting(featuresPane,"fuel-protections");
            for(int tab=0;tab<7;tab++){featuresTabs.getSelectionModel().select(tab);scene.getRoot().applyCss();scene.getRoot().layout();snapshot(scene,"console-features-"+tab);}
            ((ToggleButton)scene.lookup("#navigate-14")).fire();scene.getRoot().applyCss();scene.getRoot().layout();
            var functionPane=(IoAssignmentsPane)scene.lookup("#io-assignments-pane");
            check(((Button)scene.lookup("#io-functions-read")).isDisabled()&&((Button)scene.lookup("#io-functions-save")).isDisabled(),"Offline function writes are blocked");
            var advancedRoutes=(javafx.scene.control.TitledPane)scene.lookup("#io-functions-advanced");check(!advancedRoutes.isExpanded(),"Full terminal matrix is hidden by default");
            @SuppressWarnings("unchecked") var quickMap=(ComboBox<Integer>)scene.lookup("#io-quick-ANALOG-1");
            check(quickMap.getItems().size()==2&&quickMap.getConverter().toString(1).contains("Branco 35/13"),"MAP shows one conventional recommendation and disabled");
            advancedRoutes.setExpanded(true);scene.getRoot().applyCss();scene.getRoot().layout();
            @SuppressWarnings("unchecked") var analogFunction=(ComboBox<io.tunuino.calibration.IoAssignments.Function>)scene.lookup("#io-function-ANALOG-0");
            check(analogFunction.getItems().stream().anyMatch(f->f.label().startsWith("MAP"))&&analogFunction.getItems().stream().noneMatch(f->f.label().contains("Injetor")),"Analog inputs list sensor functions, never injector outputs");
            analogFunction.setValue(analogFunction.getItems().stream().filter(f->f.id()==3).findFirst().orElseThrow());check(functionPane.candidate().function(io.tunuino.calibration.IoAssignments.Group.ANALOG,0)==3,"Selecting TPS changes local function only");
            quickMap.setValue(1);check(functionPane.candidate().function(io.tunuino.calibration.IoAssignments.Group.ANALOG,0)==3&&quickMap.getValue()==0,"Quick selection does not steal an occupied sensor input");
            analogFunction.getSelectionModel().selectFirst();
            var functionTabs=(javafx.scene.control.TabPane)scene.lookup("#io-functions-tabs");
            advancedRoutes.setExpanded(false);auditRouting(functionPane,"io-functions");var quickTabs=(javafx.scene.control.TabPane)scene.lookup("#io-quick-tabs");
            for(int tab=0;tab<3;tab++){quickTabs.getSelectionModel().select(tab);scene.getRoot().applyCss();scene.getRoot().layout();snapshot(scene,"console-quick-routing-"+tab);}
            ((ToggleButton)scene.lookup("#navigate-11")).fire();scene.getRoot().applyCss();scene.getRoot().layout();
            var outputPane=(OutputTestPane)scene.lookup("#output-test-pane");
            var outputTabs=(javafx.scene.control.TabPane)scene.lookup("#output-test-tabs");
            outputPane.update(false,false,new io.tunuino.protocol.EngineOutputClient.State(0,0,0,0,true,1,true,1,0,true,false),
                engineBytes,"Perfil sintético de teste da interface · sem ECU física");
            check(outputTabs.getTabs().size()==3,"Separate injector, ignition and relay test tabs");
            String[] testButtons={"#test-injector-start","#test-ignition-start","#test-aux-start"};
            for(int tab=0;tab<3;tab++){
                outputTabs.getSelectionModel().select(tab);scene.getRoot().applyCss();scene.getRoot().layout();
                check(((Button)scene.lookup(testButtons[tab])).isDisabled(),"Offline test cannot drive an actuator");
                if (tab < 2) {
                    @SuppressWarnings("unchecked") var channel=(ComboBox<Integer>)scene.lookup(tab==0?"#test-injector-channel":"#test-ignition-channel");
                    check(channel.getItems().equals(List.of(1)),"Test choices contain only the configured cylinder");
                    check(channel.getConverter().toString(1).startsWith(tab==0?"Injetor 1 · Preto 35":"Ignição 1 · Preto 35"),
                        "Actuator tests show the cylinder function and physical connector");
                } else {
                    @SuppressWarnings("unchecked") var function=(ComboBox<String>)scene.lookup("#test-aux-function");
                    check(function.getItems().equals(List.of("Bomba de combustível","Ventoinha","Relé principal")),
                        "Relay tests offer three actual functions rather than all physical outputs");
                }
                snapshot(scene,"console-output-test-"+tab);
            }
            check(scene.lookup("#config-read")==null,"Actuator-test workspace is independent of sensor calibration");
            ((ToggleButton)scene.lookup("#navigate-6")).fire();scene.getRoot().applyCss();scene.getRoot().layout();
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
            var inputTabs=(javafx.scene.control.TabPane)scene.lookup("#config-form-tabs");
            inputTabs.getSelectionModel().selectLast();scene.getRoot().applyCss();scene.getRoot().layout();
            @SuppressWarnings("unchecked") var table=(javafx.scene.control.TableView<ConfigurationPane.AdcRow>)scene.lookup("#config-adc-table");
            check(table.getItems().size()==16&&table.getItems().get(0).volts().equals("1.612"),"All ADC channels and actual MCU scaling displayed");
            configEditor.showInputs(new io.tunuino.protocol.IoClient.Inputs(new LogSample(200,0,0,0,0,0),0,3300,1,0,0,new int[16]));
            check(table.getItems().stream().allMatch(row->row.raw().equals("—")),"Unavailable ADC is not displayed as zero");
            ((Label)scene.lookup("#config-feedback")).setText("Dados sintéticos de teste da interface · sem ECU física");
            snapshot(scene,"console-calibration");
            check(inputTabs.getTabs().size()==8,"Inputs are divided into trigger, MAP, lambda, temperature and ADC pages");
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
            CalibrationViewer.show(stage,"resize-16x16.tnmap",io.tunuino.calibration.BaseMaps.create(
                io.tunuino.calibration.BaseMaps.Template.NATURALLY_ASPIRATED,CalibrationMap.Kind.VE));
            Stage editor=Window.getWindows().stream().filter(w->w!=stage&&w instanceof Stage&&w.isShowing()
                &&((Stage)w).getTitle().contains("resize-16x16")).map(w->(Stage)w).findFirst().orElseThrow();
            check(editor.isResizable(),"Map editor exposes native resize and maximize controls");
            editor.setWidth(740);editor.setHeight(520);
            later(()->{
                check(editor.getScene().getWidth()<800&&editor.getScene().getHeight()<540,
                    "Native map editor actually shrinks across layout pulses");
                var grid=(javafx.scene.control.TableView<?>)editor.getScene().lookup(".table-view");
                check(grid.getColumns().size()==17&&grid.getItems().size()==16,
                    "Resizing retains every map row and column");
                snapshot(editor.getScene(),"console-map-editor-resized");
                editor.setWidth(1240);editor.setHeight(780);
                later(()->{
                    check(editor.getScene().getWidth()>1100,"Native editor grows again after shrinking");
                    check(CalibrationViewer.closeAll(),"Resizing does not modify calibration data");
                    checkFirmwareStability(stage);
                });
            });
        }

        private void checkFirmwareStability(Stage main) {
            var portA=new io.tunuino.maintenance.WindowsDfu.UsbPort("USB1","TEST-A");
            var portB=new io.tunuino.maintenance.WindowsDfu.UsbPort("USB2","TEST-B");
            var found=new FirmwarePanel.Discovery(List.of(new io.tunuino.maintenance.WindowsDfu.Device("TEST",0,"Synthetic DFU")),List.of(portA,portB),"");
            var available=new java.util.concurrent.atomic.AtomicBoolean(true);
            var scans=new java.util.concurrent.atomic.AtomicInteger();
            Stage probe=new Stage();
            FirmwarePanel panel=new FirmwarePanel(probe,available::get,()->null,()->true,true,tool->{scans.incrementAndGet();Thread.sleep(100);return found;});
            probe.setScene(new Scene(panel,900,600));ConsoleBranding.applyTheme(probe.getScene());probe.show();
            @SuppressWarnings("unchecked") var choices=(ComboBox<io.tunuino.maintenance.WindowsDfu.UsbPort>)probe.getScene().lookup("#firmware-dfu-devices");
            var select=(Button)probe.getScene().lookup("#firmware-select-image");
            panel.scan();check(!select.isDisabled()&&!choices.isDisabled(),"Background DFU discovery must not disable configuration controls");
            later(()->finishFirmwareProbe(main,probe,panel,choices,found,available,scans,0));
        }
        private void finishFirmwareProbe(Stage main,Stage probe,FirmwarePanel panel,
                ComboBox<io.tunuino.maintenance.WindowsDfu.UsbPort> choices,FirmwarePanel.Discovery found,
                java.util.concurrent.atomic.AtomicBoolean available,java.util.concurrent.atomic.AtomicInteger scans,int attempt) throws Exception {
            if(choices.getItems().size()!=2&&attempt<12){later(()->finishFirmwareProbe(main,probe,panel,choices,found,available,scans,attempt+1));return;}
            check(choices.getItems().size()==2,"Synthetic DFU discovery completed");choices.setValue(found.ports().get(1));
            var changes=new java.util.concurrent.atomic.AtomicInteger();choices.getItems().addListener((javafx.collections.ListChangeListener<io.tunuino.maintenance.WindowsDfu.UsbPort>)c->changes.incrementAndGet());
            choices.show();for(int i=0;i<10;i++)panel.showDiscovery(found,null);
            check(changes.get()==0&&choices.getValue().equals(found.ports().get(1))&&choices.isShowing(),"Unchanged DFU polls preserve choices, selection and open popup");
            choices.hide();available.set(false);int before=scans.get();panel.scan();check(scans.get()==before,"DFU discovery pauses while COM is in use");
            System.out.println("Firmware UI: background discovery, unchanged device lists, selected serial, open popup and COM exclusion passed. Synthetic discovery.");
            snapshot(probe.getScene(),"console-firmware-stability");panel.close();probe.close();finish(main);
        }

        private void finish(Stage stage) {
            console.stop(); stage.close();
            System.out.println("Console UI: actual shell, shared theme, all workspaces, channel selection, cursor, playback, missing data, compact layouts and map editor passed. Synthetic data; no USB devices.");
            Platform.exit();
        }
    }
}
