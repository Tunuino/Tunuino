/* Copyright (c) 2026 Tunuino. */
package io.tunuino.console;

import io.tunuino.datalog.CsvLogReader;
import io.tunuino.datalog.LogSample;
import io.tunuino.transport.PacketSession;
import io.tunuino.transport.UsbSerialPorts;
import io.tunuino.protocol.DeviceIdentity;
import io.tunuino.protocol.ConfigurationClient;
import io.tunuino.protocol.IoClient;
import io.tunuino.protocol.EnginePreviewClient;
import io.tunuino.protocol.EngineTimingClient;
import io.tunuino.protocol.IdentityClient;
import io.tunuino.protocol.LiveDataClient;
import io.tunuino.protocol.MapReadClient;
import io.tunuino.datalog.LiveDataMonitor;
import java.util.Optional;
import io.tunuino.calibration.CalibrationMap;
import java.io.File;
import java.time.Duration;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Callable;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;
import java.util.function.Consumer;
import javafx.application.Application;
import javafx.application.Platform;
import javafx.concurrent.Task;
import javafx.geometry.Insets;
import javafx.scene.Scene;
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
    private record DiagnosticSnapshot(ConfigurationClient.Validation validation,IoClient.Inputs inputs) {}

    private final ExecutorService fileWorker = Executors.newSingleThreadExecutor(r -> {
        Thread thread = new Thread(r, "tunuino-file-reader");
        thread.setDaemon(true);
        return thread;
    });
    private FirmwarePanel firmwarePanel;
    private Label status;
    private ConsoleShell shell;
    private javafx.animation.Timeline portRefresh;
    private Task<List<LogSample>> activeRead;
    private DeviceIdentity identifiedDevice;
    private boolean fileReading;
    private boolean monitoringActive;
    private boolean recordingPending;
    private LiveDataMonitor liveMonitor;
    private TelemetryPane telemetry;
    private DashboardPane dashboard;
    private DashboardPane fuelTables;
    private DashboardPane ignitionTables;
    private ConfigurationPane configurationPane;
    private EngineControlPane engineControlPane;
    private AuxControlPane auxControlPane;
    private FeatureControlPane featureControlPane;
    private IoAssignmentsPane ioAssignmentsPane;
    private AutoTunePane autoTunePane;
    private EngineProjectPane projectPane;
    private boolean engineArmed;
    private boolean engineAutonomous;
    private boolean serialBusy;
    private boolean portScanning;
    private final ExecutorService discoveryWorker=Executors.newSingleThreadExecutor(r->{Thread t=new Thread(r,"tunuino-port-discovery");t.setDaemon(true);return t;});
    private boolean configurationBusy;
    private Runnable refreshControls = () -> {};
    private long monitorGeneration;
    private final AtomicBoolean keepMonitoring = new AtomicBoolean();
    private final AtomicBoolean stopping = new AtomicBoolean();
    private final AtomicReference<PacketSession> connection = new AtomicReference<>();
    private final ExecutorService serialWorker = Executors.newSingleThreadExecutor(r -> {
        Thread thread = new Thread(r, "tunuino-usb");
        thread.setDaemon(true);
        return thread;
    });

    private void updateMapViews(CalibrationMap map) {
        if(map.rows()!=16||map.columns()!=16) {
            return;
        }
        for(DashboardPane page : new DashboardPane[]{dashboard,fuelTables,ignitionTables})
            if(page!=null)page.showMap(map);
        fuelTables.selectKind(CalibrationMap.Kind.VE);
        ignitionTables.selectKind(CalibrationMap.Kind.IGNITION);
    }

    private void openEcuMapEditor(Stage owner,CalibrationMap map,int row,int column,
            PacketSession session,DeviceIdentity device) {
        openMapEditor(owner,map,row,column,session,device,device.family()+" · "+map.kind()+" · cópia lida da ECU");
    }
    private void openBaseMap(Stage owner,CalibrationMap.Kind kind,io.tunuino.calibration.BaseMaps.Template template){
        CalibrationMap map=io.tunuino.calibration.BaseMaps.create(template,kind);
        PacketSession session=connection.get();DeviceIdentity device=identifiedDevice;
        String title=template+" · "+kind+" · modelo local, não lido da ECU";
        if(session==null||!session.isOpen()||device==null)CalibrationViewer.show(owner,title,map);
        else openMapEditor(owner,map,0,0,session,device,title);
    }
    private void openMapEditor(Stage owner,CalibrationMap map,int row,int column,PacketSession session,DeviceIdentity device,String title){
        java.util.function.Function<CalibrationMap,java.util.concurrent.CompletableFuture<Void>> upload=null;
        if(device.capabilities()==15&&device.firmwareVersion().equals("0.2.0"))upload=updated->{
            var completion=new java.util.concurrent.CompletableFuture<Void>();
            if(stopping.get()||engineArmed||serialBusy||monitoringActive||fileReading||configurationBusy
                    ||firmwarePanel.isBusy()||connection.get()!=session||!session.isOpen()
                    ||identifiedDevice!=device) {
                completion.completeExceptionally(new IllegalStateException(
                    "Para as outras operações e mantém a mesma sessão e identificação da ECU para guardar o mapa"));
                return completion;
            }
            status.setText("A guardar e verificar o mapa na ECU…");
            serialTask(()->{
                io.tunuino.protocol.MapWriteClient.write(session,device,updated,Duration.ofSeconds(15));
                return true;
            },ignored->{
                updateMapViews(updated);
                status.setText("Mapa guardado e verificado na ECU");
                refreshControls.run();completion.complete(null);
            },error->{
                if(!session.isOpen()){connection.compareAndSet(session,null);identifiedDevice=null;}
                for(DashboardPane page:new DashboardPane[]{dashboard,fuelTables,ignitionTables})page.clearMaps();
                status.setText("Falha ao guardar mapa: "+error.getMessage());
                refreshControls.run();completion.completeExceptionally(error);
            });
            return completion;
        };
        CalibrationViewer.show(owner,title,map,row,column,upload);
    }

    private <T> void serialTask(Callable<T> operation, Consumer<T> success, Consumer<Throwable> failure) {
        serialBusy = true;
        refreshControls.run();
        Task<T> task = new Task<>() {
            @Override protected T call() throws Exception {
                return operation.call();
            }
        };
        task.setOnSucceeded(event -> {
            serialBusy = false;
            if (!stopping.get()) {
                success.accept(task.getValue());
            }
        });
        task.setOnFailed(event -> {
            serialBusy = false;
            if (!stopping.get()) {
                failure.accept(task.getException());
            }
        });
        serialWorker.submit(task);
    }

    @Override
    public void start(Stage stage) {
        start(stage, false);
    }

    /** Used by the interface check without enumerating or opening physical devices. */
    void start(Stage stage, boolean preview) {
        if (!preview && !System.getProperty("os.name", "").toLowerCase(Locale.ROOT).startsWith("windows")) {
            throw new IllegalStateException("Tunuino Console destina-se ao Windows");
        }
        stage.setTitle("Tunuino Console · " + io.tunuino.maintenance.BuildInfo.label());
        ConsoleBranding.applyIcon(stage);
        ComboBox<DeviceIdentity.Family> hardware = new ComboBox<>();
        hardware.getItems().addAll(DeviceIdentity.Family.values());
        hardware.setValue(DeviceIdentity.Family.PROTEUS);
        Button open = new Button("Abrir datalog CSV");
        Button openMap = new Button("Abrir mapa");
        ComboBox<CalibrationMap.Kind> mapKind = new ComboBox<>();
        mapKind.getItems().addAll(CalibrationMap.Kind.values());
        mapKind.setValue(CalibrationMap.Kind.VE);
        Button readMap = new Button("Ler mapa da ECU");
        Button previewMaps = new Button("Pré-visualizar VE e ignição");
        previewMaps.setId("engine-preview");
        Label previewResult = new Label("Sem pré-visualização; saídas do motor desativadas.");
        previewResult.setId("engine-preview-result");
        previewResult.setWrapText(true);
        previewResult.getStyleClass().add("muted");
        Button modelTiming = new Button("Calcular tempos e carga teórica");
        modelTiming.setId("engine-timing");
        Label modelResult = new Label("Modelo temporal por consultar; não comanda atuadores.");
        modelResult.setId("engine-timing-result");
        modelResult.setWrapText(true);
        modelResult.getStyleClass().add("muted");
        status = new Label("Offline · nenhuma ECU identificada");
        Button readDiagnostics = new Button("Ler diagnóstico da ECU");
        readDiagnostics.setId("diagnostics-read");
        readDiagnostics.getStyleClass().add("primary");
        Label diagnosticOutput=new Label("Sem diagnóstico · liga e identifica a ECU.");
        diagnosticOutput.setId("diagnostics-output");
        diagnosticOutput.setWrapText(true);
        diagnosticOutput.getStyleClass().add("muted");
        Label identityDetails = new Label("Identificação da ECU pendente");
        ComboBox<UsbSerialPorts.Port> ports = new ComboBox<>();
        ports.setPromptText("Selecionar porta USB");
        ports.setPrefWidth(210);
        Button scan = new Button("Atualizar portas");
        Button connect = new Button("Ligar USB");
        Button identify = new Button("Identificar ECU");
        Button monitor = new Button("Ver dados em tempo real");
        Button topMonitor = new Button("Tempo real");topMonitor.setId("monitor-header");topMonitor.disableProperty().bind(monitor.disabledProperty());topMonitor.setOnAction(e->monitor.fire());
        Button capture = new Button("Gravar datalog");
        connect.getStyleClass().add("primary"); monitor.getStyleClass().add("primary");
        open.setId("open-datalog"); openMap.setId("open-map");
        monitor.setId("monitor-live"); capture.setId("record-csv");
        connect.setId("connect-usb");
        identify.setDisable(true);
        connect.setDisable(true);
        Runnable refreshConnection = () -> {
            if (firmwarePanel != null) firmwarePanel.refreshControls();
            PacketSession current = connection.get();
            boolean connected = current != null && current.isOpen();
            if(!connected||identifiedDevice==null)engineArmed=false;
            if (shell != null) shell.connectionState(identifiedDevice, connected);
            if(dashboard!=null) {
                boolean mapsAvailable=!engineArmed&&!serialBusy&&!monitoringActive&&!fileReading
                    &&(firmwarePanel==null||!firmwarePanel.isBusy());
                for(DashboardPane page : new DashboardPane[]{dashboard,fuelTables,ignitionTables}) {
                    if(page==null)continue;
                    page.setPort(ports.getValue()==null?null:ports.getValue().name());
                    page.setConnection(identifiedDevice,connected,mapsAvailable);
                }
            }
            if(configurationPane!=null)configurationPane.availability(identifiedDevice,connected,
                engineArmed||serialBusy||monitoringActive||fileReading||(firmwarePanel!=null&&firmwarePanel.isBusy()));
            if(auxControlPane!=null)auxControlPane.availability(identifiedDevice,connected,serialBusy||monitoringActive||fileReading||(firmwarePanel!=null&&firmwarePanel.isBusy()),engineArmed);
            if(featureControlPane!=null)featureControlPane.availability(identifiedDevice,connected,serialBusy||monitoringActive||fileReading||(firmwarePanel!=null&&firmwarePanel.isBusy()),engineArmed);
            if(ioAssignmentsPane!=null)ioAssignmentsPane.availability(identifiedDevice,connected,serialBusy||monitoringActive||fileReading||(firmwarePanel!=null&&firmwarePanel.isBusy()),engineArmed);
            if(engineControlPane!=null)engineControlPane.availability(identifiedDevice,connected,
                serialBusy||monitoringActive||fileReading||(firmwarePanel!=null&&firmwarePanel.isBusy()));
            if(projectPane!=null)projectPane.availability(identifiedDevice,connected,
                serialBusy||monitoringActive||fileReading||(firmwarePanel!=null&&firmwarePanel.isBusy()),engineArmed);
            if(autoTunePane!=null)autoTunePane.availability(identifiedDevice,connected,
                serialBusy||fileReading||(firmwarePanel!=null&&firmwarePanel.isBusy()),monitoringActive,engineArmed);
            connect.setText(connected ? "Desligar USB" : "Ligar USB");
            boolean programming = firmwarePanel != null && firmwarePanel.isBusy();
            connect.setDisable(programming || serialBusy || monitoringActive || (!connected && ports.getValue() == null));
            scan.setDisable(preview || programming || serialBusy || connected || monitoringActive);
            ports.setDisable(programming || serialBusy || connected || monitoringActive);
            hardware.setDisable(preview || serialBusy || connected || monitoringActive);
            identify.setDisable(serialBusy || !connected || monitoringActive);
            readDiagnostics.setDisable(preview||serialBusy||monitoringActive||fileReading
                ||(firmwarePanel!=null&&firmwarePanel.isBusy())||!connected
                ||!EnginePreviewClient.supports(identifiedDevice));
            LiveDataMonitor.Recording recording = liveMonitor == null ? null : liveMonitor.recording();
            capture.setText(recording != null ? "Parar gravação CSV" : "Gravar datalog CSV");
            monitor.setText(monitoringActive ? "Parar monitorização" : "Ver dados em tempo real");
            topMonitor.setText(monitoringActive?"Parar dados":"Tempo real");
            boolean liveAvailable = identifiedDevice != null
                && (identifiedDevice.capabilities() & LiveDataClient.CAPABILITY) != 0;
            monitor.setDisable((engineArmed&&!engineAutonomous)||programming || (monitoringActive ? !keepMonitoring.get()
                : serialBusy || !connected || !liveAvailable || fileReading));
            capture.setDisable(!monitoringActive || !keepMonitoring.get() || recordingPending);
            boolean mapAvailable = identifiedDevice != null
                && (identifiedDevice.capabilities() & MapReadClient.CAPABILITY) != 0;
            readMap.setDisable(engineArmed||serialBusy || monitoringActive || fileReading || !connected || !mapAvailable);
            previewMaps.setDisable(preview || programming || serialBusy || monitoringActive
                || fileReading || !connected || !EnginePreviewClient.supports(identifiedDevice));
            modelTiming.setDisable(preview || programming || serialBusy || monitoringActive
                || fileReading || !connected || !EnginePreviewClient.supports(identifiedDevice));
            mapKind.setDisable(serialBusy || monitoringActive || fileReading);
            open.setDisable(monitoringActive || fileReading);
            openMap.setDisable(monitoringActive || fileReading);
        };
        refreshControls = refreshConnection;
        Consumer<Throwable> usbFailure = error -> {
            identifiedDevice = null;
            identityDetails.setText("Identificação da ECU pendente");
            previewResult.setText("Sem pré-visualização; ligação USB indisponível.");
            modelResult.setText("Modelo temporal indisponível: ligação USB encerrada.");
            status.setText("Falha USB: " + error.getMessage());
            refreshConnection.run();
        };
        ports.setOnAction(event -> refreshConnection.run());
        hardware.setOnAction(event -> refreshConnection.run());
        scan.setOnAction(event -> {
            capture.setDisable(true);
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
            if (firmwarePanel != null && firmwarePanel.isBusy()) return;
            capture.setDisable(true);
            identifiedDevice = null;
            identityDetails.setText("Identificação da ECU pendente");
            previewResult.setText("Sem pré-visualização; identifica novamente a ECU.");
            modelResult.setText("Modelo temporal por consultar; identifica novamente a ECU.");
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
                    if (opened) identify.fire();
                }, usbFailure);
            } else {
                refreshConnection.run();
            }
        });
        identify.setOnAction(event -> {
            capture.setDisable(true);
            PacketSession current = connection.get();
            if (current == null || !current.isOpen()) {
                refreshConnection.run();
                return;
            }
            DeviceIdentity.Family expected = hardware.getValue();
            identify.setDisable(true);
            connect.setDisable(true);
            status.setText("A identificar a ECU…");
            identifiedDevice = null;
            identityDetails.setText("Identificação da ECU pendente");
            serialTask(() -> {
                try {
                    return IdentityClient.identify(current, expected, Duration.ofSeconds(3));
                } catch (java.io.IOException error) {
                    connection.compareAndSet(current, null);
                    throw error;
                }
            }, identity -> {
                identifiedDevice = identity;
                previewResult.setText("Pré-visualização por consultar; saídas do motor desativadas.");
                modelResult.setText("Modelo temporal por consultar; saídas do motor desativadas.");
                identityDetails.setText(identity.family() + " · " + identity.mcuSeries()
                    + " · revisão " + identity.hardwareRevision()
                    + " · firmware " + identity.firmwareVersion() + " · UID " + identity.uid());
                status.setText((identity.capabilities() & LiveDataClient.CAPABILITY) != 0
                    ? "ECU identificada · dados em tempo real disponíveis"
                    : "ECU identificada · firmware sem dados em tempo real");
                refreshConnection.run();
            }, usbFailure);
        });
        telemetry = new TelemetryPane();
        dashboard = new DashboardPane();
        fuelTables = new DashboardPane(true);
        ignitionTables = new DashboardPane(true);
        fuelTables.selectKind(CalibrationMap.Kind.VE);
        ignitionTables.selectKind(CalibrationMap.Kind.IGNITION);
        Label summary = new Label("Escolhe um datalog local para analisar.");
        identityDetails.setWrapText(true);
        summary.getStyleClass().add("muted");
        summary.setWrapText(true); summary.setPadding(new Insets(4, 24, 12, 24));
        BorderPane root = new BorderPane(telemetry);
        javafx.scene.layout.FlowPane dataActions = ConsoleShell.actions(monitor, capture, open);
        dataActions.getStyleClass().add("data-actions"); root.setTop(dataActions);
        root.setBottom(summary);
        firmwarePanel = new FirmwarePanel(stage,
            () -> connection.get() == null && !serialBusy && !monitoringActive,
            () -> ports.getValue() == null ? null : ports.getValue().name(),
            () -> hardware.getValue() == DeviceIdentity.Family.PROTEUS, !preview);
        portRefresh = new javafx.animation.Timeline(new javafx.animation.KeyFrame(
            javafx.util.Duration.seconds(3), event -> {
                if(portScanning||serialBusy||monitoringActive||connection.get()!=null||firmwarePanel.isBusy())return;
                portScanning=true;
                discoveryWorker.submit(()->{
                    try{
                        var found=UsbSerialPorts.list();
                        Platform.runLater(()->{
                            portScanning=false;if(stopping.get()||connection.get()!=null||serialBusy)return;
                            if(!ports.getItems().equals(found)){
                                UsbSerialPorts.Port previous=ports.getValue();ports.getItems().setAll(found);
                                ports.setValue(previous==null?null:found.stream().filter(p->p.path().equals(previous.path())).findFirst().orElse(null));
                                refreshConnection.run();
                            }
                        });
                    }catch(Exception error){Platform.runLater(()->{portScanning=false;if(!stopping.get()&&connection.get()==null)status.setText("Pesquisa de portas: "+error.getMessage());});}
                });
            }));
        portRefresh.setCycleCount(javafx.animation.Animation.INDEFINITE);
        if (!preview) portRefresh.play();
        previewMaps.setOnAction(event -> {
            PacketSession current = connection.get();
            DeviceIdentity device = identifiedDevice;
            if (serialBusy || monitoringActive || fileReading || current == null
                    || !current.isOpen() || !EnginePreviewClient.supports(device)) {
                refreshConnection.run();
                return;
            }
            previewResult.setText("A consultar cálculo da ECU…");
            status.setText("A consultar mapas VE e ignição sem comandar saídas…");
            serialTask(() -> EnginePreviewClient.read(current, device, Duration.ofSeconds(3)),
                result -> {
                    if (result.valid()) {
                        previewResult.setText(String.format(Locale.ROOT,
                            "VE %.2f %% · ignição %.2f ° · RPM %.1f · MAP %.1f kPa · revisão %d."
                            + " Pré-visualização apenas, sem atuação.",
                            result.vePercent(), result.advanceDegrees(), result.rpm(),
                            result.mapKpa(), result.generation()));
                    } else {
                        previewResult.setText("Cálculo indisponível: " + result.explanation()
                            + ". Nenhuma saída de motor foi acionada.");
                    }
                    status.setText(result.valid() ? "Pré-visualização da ECU recebida"
                        : "Cálculo bloqueado por condições de validade");
                    refreshConnection.run();
                }, error -> {
                    previewResult.setText("Não foi possível consultar a ECU: " + error.getMessage());
                    if (!current.isOpen()) {
                        connection.compareAndSet(current, null);
                        identifiedDevice = null;
                    }
                    status.setText("Falha de leitura: " + error.getMessage());
                    refreshConnection.run();
                });
        });
        modelTiming.setOnAction(event -> {
            PacketSession current = connection.get();
            DeviceIdentity device = identifiedDevice;
            if(serialBusy||monitoringActive||fileReading||current==null
                    ||!current.isOpen()||!EnginePreviewClient.supports(device)) {
                refreshConnection.run();return;
            }
            modelResult.setText("A calcular intervalos teóricos na ECU…");
            status.setText("A consultar modelo temporal de bancada…");
            serialTask(()->EngineTimingClient.read(current,device,Duration.ofSeconds(3)),result->{
                if(result.valid()) {
                    modelResult.setText(String.format(Locale.ROOT,
                        "RPM %.1f · volta %.3f ms · ciclo 720° %.3f ms · dente %.3f ms"
                        + " · índice relativo de carga %.2f %% · avanço relativo %+.3f ms."
                        + " Apenas cálculo matemático, sem fase PMS nem saídas.",
                        result.rpm(),result.revolutionUs()/1000.0,result.cycleUs()/1000.0,
                        result.wheelSlotUs()/1000.0,result.relativeLoadPercent(),
                        result.sparkReferenceUs()/1000.0));
                } else modelResult.setText("Modelo bloqueado: "+result.explanation()
                    + ". Nenhum tempo de atuação foi calculado.");
                status.setText(result.valid()?"Cálculo temporal teórico recebido"
                    :"Temporização indisponível por falta de condições");
                refreshConnection.run();
            },error->{
                modelResult.setText("Modelo indisponível: "+error.getMessage());
                if(!current.isOpen()){connection.compareAndSet(current,null);identifiedDevice=null;}
                status.setText("Consulta de temporização: "+error.getMessage());
                refreshConnection.run();
            });
        });
        readMap.setOnAction(event -> {
            PacketSession current = connection.get();
            DeviceIdentity device = identifiedDevice;
            CalibrationMap.Kind kind = mapKind.getValue();
            if (serialBusy || monitoringActive || fileReading || current == null || !current.isOpen()
                    || device == null || kind == null || (device.capabilities() & MapReadClient.CAPABILITY) == 0) {
                refreshConnection.run();
                return;
            }
            status.setText("A ler mapa da ECU…");
            serialTask(() -> MapReadClient.read(current, device, kind, Duration.ofSeconds(5)), map -> {
                updateMapViews(map);
                openEcuMapEditor(stage,map,0,0,current,device);
                status.setText("Mapa validado · podes consultar, editar e guardar uma cópia offline");
                refreshConnection.run();
            }, error -> {
                connection.compareAndSet(current, null);
                usbFailure.accept(error);
            });
        });
        openMap.setOnAction(event -> {
            FileChooser chooser = new FileChooser();
            chooser.setTitle("Abrir mapa Tunuino");
            ConsoleFiles.in(chooser, io.tunuino.maintenance.AppPaths.maps());
            chooser.getExtensionFilters().add(new FileChooser.ExtensionFilter("Mapa Tunuino", "*.tnmap"));
            File selected = chooser.showOpenDialog(stage);
            if (selected == null) { return; }
            fileReading = true;
            refreshConnection.run();
            Task<CalibrationMap> task = new Task<>() {
                @Override protected CalibrationMap call() throws Exception {
                    return CalibrationMap.read(selected.toPath());
                }
            };
            task.setOnSucceeded(done -> {
                fileReading = false;
                if (!stopping.get()) {
                    CalibrationViewer.show(stage, selected.getName(), task.getValue());
                    refreshConnection.run();
                }
            });
            task.setOnFailed(done -> {
                fileReading = false;
                if (!stopping.get()) {
                    Alert alert = ConsoleBranding.alert(Alert.AlertType.ERROR, task.getException().getMessage());
                    alert.initOwner(stage);
                    alert.setHeaderText("Mapa inválido");
                    alert.showAndWait();
                    refreshConnection.run();
                }
            });
            fileWorker.submit(task);
        });
        monitor.setOnAction(event -> {
            if (monitoringActive) {
                keepMonitoring.set(false);
                status.setText("A terminar a monitorização e a fechar o datalog…");
                refreshConnection.run();
                return;
            }
            PacketSession current = connection.get();
            DeviceIdentity device = identifiedDevice;
            if (serialBusy || current == null || !current.isOpen() || device == null
                    || (device.capabilities() & LiveDataClient.CAPABILITY) == 0 || fileReading) {
                refreshConnection.run();
                return;
            }
            monitoringActive = true;
            keepMonitoring.set(true);
            long generation = ++monitorGeneration;
            LiveDataMonitor reading = new LiveDataMonitor(current, device, Duration.ofMillis(100));
            liveMonitor = reading;
            telemetry.beginLive();
            dashboard.monitoringStarted();
            summary.setText("Monitorização ativa · CSV desligado");
            status.setText("A receber dados da ECU…");
            AtomicReference<Optional<LogSample>> latest = new AtomicReference<>();
            AtomicBoolean renderPending = new AtomicBoolean();
            Consumer<Optional<LogSample>> renderSample = sample -> {
                latest.set(sample);
                if (!stopping.get() && renderPending.compareAndSet(false, true)) {
                    Platform.runLater(() -> {
                        renderPending.set(false);
                        if (stopping.get() || !monitoringActive || monitorGeneration != generation) return;
                        Optional<LogSample> value = latest.getAndSet(null);
                        if(value!=null) {
                            telemetry.showLive(value);
                            dashboard.showSample(value);
                        }
                        LiveDataMonitor.Recording recording = reading.recording();
                        summary.setText(recording == null ? "Monitorização ativa · CSV desligado"
                            : "CSV: " + recording.path().getFileName() + " · " + recording.samples() + " amostras gravadas");
                        refreshConnection.run();
                    });
                }
            };
            serialTask(() -> reading.run(keepMonitoring::get, renderSample, error -> Platform.runLater(() -> {
                if (stopping.get() || liveMonitor != reading) return;
                status.setText("Gravação interrompida: " + error.getMessage());
                refreshConnection.run();
            }), extra -> Platform.runLater(()->{
                if(stopping.get()||!monitoringActive||monitorGeneration!=generation)return;
                engineArmed=extra.outputs().armed();engineAutonomous=extra.outputs().autonomous();
                dashboard.showAuxiliary(extra.sensors());dashboard.showOutputs(extra.outputs(),extra.profile());
                dashboard.showCam(extra.cam());
            })), count -> {
                monitoringActive = false;
                keepMonitoring.set(false);
                liveMonitor = null;
                telemetry.endLive("Monitorização terminada · histórico do gráfico mantido");
                dashboard.monitoringStopped("Monitorização terminada · dados atuais indisponíveis.");
                summary.setText(count + " amostras válidas recebidas · ficheiros CSV encerrados");
                status.setText("ECU ligada · monitorização parada");
                refreshConnection.run();
            }, error -> {
                monitoringActive = false;
                keepMonitoring.set(false);
                liveMonitor = null;
                if (!current.isOpen()) {
                    connection.compareAndSet(current, null);
                    identifiedDevice = null;
                    identityDetails.setText("Identificação da ECU pendente");
                }
                telemetry.endLive("Monitorização interrompida · " + error.getMessage());
                dashboard.monitoringStopped("Monitorização interrompida · dados atuais indisponíveis.");
                status.setText("Falha de monitorização: " + error.getMessage());
                refreshConnection.run();
            });
        });
        capture.setOnAction(event -> {
            LiveDataMonitor reading = liveMonitor;
            if (!monitoringActive || reading == null || !keepMonitoring.get() || recordingPending) return;
            java.util.concurrent.CompletableFuture<LiveDataMonitor.Recording> request;
            boolean ending = reading.recording() != null;
            if (ending) {
                request = reading.stopRecording();
            } else {
                FileChooser chooser = new FileChooser();
                chooser.setTitle("Guardar novo datalog");
                ConsoleFiles.in(chooser, io.tunuino.maintenance.AppPaths.logs());
                chooser.getExtensionFilters().add(new FileChooser.ExtensionFilter("Datalog CSV", "*.csv"));
                chooser.setInitialFileName("tunuino-" + LocalDateTime.now()
                    .format(DateTimeFormatter.ofPattern("yyyyMMdd-HHmmss")) + ".csv");
                File destination = chooser.showSaveDialog(stage);
                if (destination == null) return;
                request = reading.startRecording(destination.toPath());
            }
            recordingPending = true;
            refreshConnection.run();
            request.whenComplete((recording, error) -> Platform.runLater(() -> {
                recordingPending = false;
                if (stopping.get()) return;
                if (error != null) status.setText("Falha no datalog: " + error.getMessage());
                else if (ending) status.setText(recording == null ? "CSV desligado"
                    : "Datalog guardado: " + recording.samples() + " amostras em " + recording.path().getFileName());
                else status.setText("A gravar CSV · monitorização continua ativa");
                refreshConnection.run();
            }));
        });
        open.setOnAction(event -> {
            FileChooser chooser = new FileChooser();
            chooser.setTitle("Abrir datalog Tunuino");
            ConsoleFiles.in(chooser, io.tunuino.maintenance.AppPaths.logs());
            chooser.getExtensionFilters().add(new FileChooser.ExtensionFilter("Datalog CSV", "*.csv"));
            File selected = chooser.showOpenDialog(stage);
            if (selected == null) {
                return;
            }
            open.setDisable(true);
            fileReading = true;
            refreshConnection.run();
            summary.setText("A ler " + selected.getName() + "…");
            activeRead = new Task<>() {
                @Override protected List<LogSample> call() throws Exception {
                    return CsvLogReader.read(selected.toPath());
                }
            };
            activeRead.setOnSucceeded(done -> {
                if (stopping.get()) return;
                List<LogSample> samples = activeRead.getValue();
                telemetry.showOffline(selected.getName(), samples);
                summary.setText(selected.getName() + " · " + samples.size()
                    + " amostras · seleciona o canal ou percorre o datalog");
                fileReading = false;
                refreshConnection.run();
            });
            activeRead.setOnFailed(done -> {
                if (stopping.get()) return;
                summary.setText("Não foi possível abrir o datalog. O gráfico anterior foi mantido.");
                Alert alert = ConsoleBranding.alert(Alert.AlertType.ERROR, activeRead.getException().getMessage());
                alert.initOwner(stage);
                alert.setHeaderText("Datalog inválido");
                alert.showAndWait();
                fileReading = false;
                refreshConnection.run();
            });
            fileWorker.submit(activeRead);
        });
        Consumer<CalibrationMap.Kind> readMapForDashboard = kind -> {
            PacketSession current=connection.get();
            DeviceIdentity device=identifiedDevice;
            if(serialBusy||monitoringActive||fileReading||firmwarePanel.isBusy()
                    ||current==null||!current.isOpen()||device==null
                    ||(device.capabilities()&MapReadClient.CAPABILITY)==0){
                status.setText("Para ler mapas, liga e identifica a ECU e pára a monitorização.");
                refreshConnection.run();return;
            }
            status.setText("A ler mapa "+kind+" de 16×16 da ECU…");
            serialTask(()->MapReadClient.read(current,device,kind,Duration.ofSeconds(7)),
                map->{updateMapViews(map);status.setText("Tabela "+kind+" 16×16 atualizada");
                    refreshConnection.run();},
                error->{
                    if(!current.isOpen()){connection.compareAndSet(current,null);identifiedDevice=null;}
                    status.setText("Não foi possível ler "+kind+": "+error.getMessage());
                    refreshConnection.run();});
        };
        dashboard.setReadAction(readMapForDashboard);
        fuelTables.setReadAction(readMapForDashboard);
        ignitionTables.setReadAction(readMapForDashboard);
        Consumer<DashboardPane.MapSelection> editMapForDashboard=selection->{
            PacketSession current=connection.get();DeviceIdentity device=identifiedDevice;
            if(serialBusy||monitoringActive||fileReading||firmwarePanel.isBusy()
                    ||current==null||!current.isOpen()||device==null) {
                status.setText("Para editar uma tabela da ECU, mantém a ligação identificada e pára as outras operações.");
                refreshConnection.run();return;
            }
            openEcuMapEditor(stage,selection.map(),selection.row(),selection.column(),current,device);
        };
        dashboard.setEditAction(editMapForDashboard);
        fuelTables.setEditAction(editMapForDashboard);
        ignitionTables.setEditAction(editMapForDashboard);
        readDiagnostics.setOnAction(event->{
            PacketSession current=connection.get();
            DeviceIdentity device=identifiedDevice;
            if(serialBusy||monitoringActive||fileReading||firmwarePanel.isBusy()
                    ||current==null||!current.isOpen()||!EnginePreviewClient.supports(device)) {
                status.setText("Para consultar o diagnóstico, identifica a ECU e pára a monitorização.");
                refreshConnection.run();return;
            }
            diagnosticOutput.setText("A ler o estado real dos sensores e da configuração…");
            serialTask(()->new DiagnosticSnapshot(
                    ConfigurationClient.validation(current,device,Duration.ofSeconds(3)),
                    IoClient.read(current,device,Duration.ofSeconds(3))),
                report->{
                    var validation=report.validation();
                    var values=report.inputs();
                    int ioStatus=values.status();
                    int valid=values.sample().validMask();
                    diagnosticOutput.setText(
                        "CONFIGURAÇÃO: "+validation.explanation()+"\n"
                        +"Flash: "+(validation.persisted()?"gravada":"não gravada")
                        +" · Alterações pendentes: "+(validation.dirty()?"sim":"não")
                        +" · geração "+validation.generation()+"\n"
                        +"ADC: "+((ioStatus&1)!=0?"válido":"indisponível")
                        +" · Referência "+values.referenceMv()+" mV\n"
                        +"Cambota: "+((ioStatus&8)!=0?"sincronização válida":"sem sincronização válida")
                        +" · Dente "+values.tooth()+" · Erros "+values.errors()+"\n"
                        +"RPM: "+((valid&1)!=0?String.format(Locale.ROOT,"%.1f",values.sample().rpm()):"—")
                        +" · MAP: "+((valid&2)!=0?String.format(Locale.ROOT,"%.1f kPa",values.sample().mapKpa()):"—")
                        +" · Lambda: "+((valid&4)!=0?String.format(Locale.ROOT,"%.3f",values.sample().lambda()):"—")
                        +" · Temperatura: "+((valid&8)!=0?String.format(Locale.ROOT,"%.1f °C",values.sample().coolantC()):"—"));
                    status.setText("Diagnóstico de leitura concluído");
                    refreshConnection.run();
                },error->{
                    diagnosticOutput.setText("Falha de diagnóstico: "+error.getMessage());
                    if(!current.isOpen()){connection.compareAndSet(current,null);identifiedDevice=null;}
                    status.setText("Diagnóstico: "+error.getMessage());
                    refreshConnection.run();
                });
        });
        javafx.scene.layout.FlowPane connectionControls = ConsoleShell.actions(
            new Label("USB"), ports, scan, connect, identify,topMonitor);
        connectionControls.getStyleClass().add("connection-controls");
        shell = new ConsoleShell(hardware, connectionControls, status, identityDetails);
        shell.addPage("Dashboard", "Dashboard da ECU", "Sensores reais e tabelas VE/ignição 16×16.",
            "M1 1H9V9H1ZM13 1H21V9H13ZM1 13H9V21H1ZM13 13H21V21H13Z", ConsoleShell.scroll(dashboard));
        ComboBox<io.tunuino.calibration.BaseMaps.Template> baseTemplate=new ComboBox<>();
        baseTemplate.getItems().addAll(io.tunuino.calibration.BaseMaps.Template.values());baseTemplate.getSelectionModel().selectFirst();
        baseTemplate.setId("base-map-template");
        Button baseVe=new Button("Criar base VE 16×16"),baseSpark=new Button("Criar base ignição 16×16");
        baseVe.setId("base-map-ve");baseSpark.setId("base-map-ignition");
        baseVe.setOnAction(e->openBaseMap(stage,CalibrationMap.Kind.VE,baseTemplate.getValue()));
        baseSpark.setOnAction(e->openBaseMap(stage,CalibrationMap.Kind.IGNITION,baseTemplate.getValue()));
        for(DashboardPane page:new DashboardPane[]{dashboard,fuelTables,ignitionTables})
            page.onBaseMap(kind->openBaseMap(stage,kind,io.tunuino.calibration.BaseMaps.Template.NATURALLY_ASPIRATED));
        VBox maps = new VBox(16,
            ConsoleShell.card("MAPAS BASE PRÉ-DEFINIDOS", "Referências editáveis para começar a afinação: 200–7000 rpm e 20–105 kPa absolutos. A base aspirada não cobre pressão de turbo. Confirma mistura e avanço para o teu motor antes de utilizar. Criar abre uma cópia local; guardar na ECU é uma ação separada.",baseTemplate,ConsoleShell.actions(baseVe,baseSpark)),
            ConsoleShell.card("FICHEIRO LOCAL", "Abre um mapa VE ou de ignição para consultar a tabela, editar valores e guardar uma cópia .tnmap.", openMap),
            ConsoleShell.card("MAPA DA CENTRALINA", "Lê uma cópia do mapa quando esta função está disponível no firmware.",
                ConsoleShell.actions(mapKind, readMap)),
            ConsoleShell.card("PRÉ-VISUALIZAÇÃO DE CÁLCULO", "Consulta os valores interpolados pelo firmware."
                + " Os resultados exigem sensores válidos e mapas calibrados; esta função não ativa injetores nem bobinas.",
                previewMaps, previewResult),
            ConsoleShell.card("MODELO TEMPORAL · SEM SAÍDAS", "Calcula o tempo de uma volta, ciclo e dente."
                + " O índice de carga é relativo; não determina combustível nem ponto de ignição físico.",
                modelTiming, modelResult));
        maps.setPadding(new Insets(0, 24, 24, 24));
        shell.addPage("Tuning", "Tuning e calibração", "Edição de mapas e pré-visualização matemática.",
            "M1 2H21V20H1ZM3 4V8H7V4ZM9 4V8H13V4ZM15 4V8H19V4ZM3 10V14H7V10ZM9 10V14H13V10ZM15 10V14H19V10ZM3 16V18H7V16ZM9 16V18H13V16ZM15 16V18H19V16Z", ConsoleShell.scroll(maps));
        shell.addPage("Fuel Tables", "Tabela de combustível · VE 16×16",
            "Mapa volumétrico real de 256 células. O comando físico exige perfil e armação explícita em Configuração do motor.",
            "M2 2H22V22H2ZM5 5H10V10H5ZM12 5H19V10H12ZM5 12H10V19H5ZM12 12H19V19H12Z",
            ConsoleShell.scroll(fuelTables));
        shell.addPage("Ignition Tables", "Tabela de ignição · 16×16",
            "Mapa real de avanço em graus. Configura PMS, canais e dwell em Configuração do motor antes de armar.",
            "M12 2L6 13H11L8 22L18 10H13Z",ConsoleShell.scroll(ignitionTables));
        ConfigurationPane.Access ecuAccess=new ConfigurationPane.Access() {
            @Override public <T> void run(ConfigurationPane.Operation<T> operation,Consumer<T> success,Consumer<Throwable> failure) {
                PacketSession current=connection.get();DeviceIdentity device=identifiedDevice;
                if(serialBusy||monitoringActive||fileReading||firmwarePanel.isBusy()||current==null||!current.isOpen()
                        ||!EnginePreviewClient.supports(device)) {
                    failure.accept(new IllegalStateException("Para a monitorização e identifica a ECU antes de configurar"));return;
                }
                configurationBusy=true;
                serialTask(()->operation.run(current,device),result->{configurationBusy=false;success.accept(result);refreshConnection.run();},error->{
                    configurationBusy=false;
                    if(!current.isOpen()){connection.compareAndSet(current,null);identifiedDevice=null;}
                    failure.accept(error);status.setText("Configuração: "+error.getMessage());refreshConnection.run();
                });
            }
        };
        autoTunePane=new AutoTunePane(stage,ecuAccess,learning->{
            LiveDataMonitor tuningMonitor=liveMonitor;if(tuningMonitor!=null)tuningMonitor.tuningObserver(learning?frame->Platform.runLater(()->{
                if(!stopping.get()&&liveMonitor==tuningMonitor)autoTunePane.observe(frame);
            }):null);
        },map->{updateMapViews(map);engineControlPane.externalConfiguration(0);});
        shell.addPage("Autotune","Autotune VE","Propostas de VE a partir da lambda wideband e das leituras USB reais.",
            "M12 3L21 12L12 21L3 12ZM11 7H13V13H11ZM11 16H13V18H11Z",ConsoleShell.scroll(autoTunePane));
        configurationPane=new ConfigurationPane(stage,ecuAccess);
        Consumer<io.tunuino.protocol.EngineOutputClient.State> outputState=value->{
            engineArmed=value.armed();
            engineAutonomous=value.autonomous();
            for(DashboardPane page:new DashboardPane[]{dashboard,fuelTables,ignitionTables})page.showOutputs(value);
            refreshControls.run();
        };
        engineControlPane=new EngineControlPane(stage,ecuAccess,outputState);
        auxControlPane=new AuxControlPane(stage,ecuAccess,outputState);
        featureControlPane=new FeatureControlPane(stage,ecuAccess,outputState);
        auxControlPane.onSnapshot(saved->{featureControlPane.sensorSaved(saved);configurationPane.auxiliarySaved(saved);engineControlPane.externalConfiguration(saved.validation().generation());});
        featureControlPane.onSnapshot(saved->{auxControlPane.sensorSaved(saved);configurationPane.auxiliarySaved(saved);engineControlPane.externalConfiguration(saved.validation().generation());});
        configurationPane.onSaved(validation->{auxControlPane.coreSaved(validation);featureControlPane.coreSaved(validation);engineControlPane.externalConfiguration(validation.generation());});
        configurationPane.addSensorPage("TPS",auxControlPane.sensorCalibration());
        configurationPane.addSensorPage("IAT · ar",featureControlPane.sensorCalibration("IAT"));
        configurationPane.addSensorPage("Tensão",featureControlPane.sensorCalibration("VOLT"));
        ioAssignmentsPane=new IoAssignmentsPane(stage,ecuAccess,value->{engineControlPane.externalState(value);outputState.accept(value);});
        shell.addPage("Data Log", "Datalogs e monitorização", "Gravação CSV e reprodução dos sinais disponíveis.",
            "M3 3H21V21H3ZM6 7H18V9H6ZM6 12H18V14H6ZM6 17H15V19H6Z", root);
        shell.addPage("Inputs / Outputs", "Sensores e calibrações", "Roda fónica, sensores e leitura das entradas ADC.",
            "M2 3H20V7H2ZM2 10H20V14H2ZM2 17H20V21H2Z", ConsoleShell.scroll(configurationPane));

        shell.addPage("Diagnostics", "Diagnósticos", "Validação e análise de sinais verdadeiros.",
            "M12 2A10 10 0 1 0 12 22A10 10 0 1 0 12 2ZM11 6H13V13H11ZM11 16H13V18H11Z",
            ConsoleShell.scroll(new VBox(12,
                ConsoleShell.card("ESTADO DA ECU",
                    "Verifica a configuração persistida, erros de captura e validade das entradas da ECU "
                    +"através de dois pedidos binários só de leitura. A consulta exige USB exclusivo.",
                    readDiagnostics, diagnosticOutput),
                ConsoleShell.card("VALIDAÇÃO DE CALIBRAÇÃO",
                    "Verifica as leituras e os parâmetros na área Sensores e entradas. "
                    +"Em Tuning estão disponíveis os cálculos VE/ignição e os motivos de bloqueio.",
                    new Label("Lê a ECU e confirma os sinais antes de qualquer afinação.")))));
        shell.addPage("Firmware", "Firmware e ligação USB", "Seleciona a imagem, verifica o dispositivo e acompanha a gravação.",
            "M5 4H17V18H5ZM7 6V16H15V6ZM1 6H4V8H1ZM1 11H4V13H1ZM18 6H21V8H18ZM18 11H21V13H18ZM7 0H9V3H7ZM13 0H15V3H13ZM7 19H9V22H7ZM13 19H15V22H13Z", ConsoleShell.scroll(firmwarePanel));
        shell.addPage("Settings", "Configurações da Console", "Identidade e funcionalidades suportadas.",
            "M12 2L15 4L19 4L20 8L22 12L20 16L19 20L15 20L12 22L9 20L5 20L4 16L2 12L4 8L5 4L9 4Z",
            ConsoleShell.scroll(new VBox(12,
                ConsoleShell.card("TUNUINO CONSOLE", "Windows · JavaFX · USB CDC",
                    new Label(io.tunuino.maintenance.BuildInfo.label())),
                ConsoleShell.card("FORMATO ATUAL", "Firmware IO 0.2.0 e mapas VE/ignição nativos 16×16.",
                    new Label("Dados dos instrumentos apenas quando recebidos da ECU.")))));
        shell.addPage("Engine", "Configuração do motor", "Modelos de cilindros e ordem de ignição, injeção, dwell, sincronização e saídas auxiliares.",
            "M3 4H19V18H3ZM0 8H3V14H0ZM19 8H22V14H19Z",ConsoleShell.scroll(engineControlPane));
        shell.addPage("Output Tests", "Testes de atuadores", "Testes separados de injetores, ignição, bomba, ventoinha e relé principal.",
            "M12 2L6 13H11L8 22L18 10H13Z",ConsoleShell.scroll(engineControlPane.tests()));
        shell.addPage("Aux Control","TPS, ralenti e turbo","Calibração TPS, válvulas PWM e controlo de rotação/pressão.","M3 4H19V18H3ZM6 8H16V14H6Z",ConsoleShell.scroll(auxControlPane));
        shell.label("Aux Control","Ralenti e turbo");
        shell.addPage("Fuel Features","Combustível e proteções","Compensações, alvo lambda, transientes, limites e ajustes por cilindro.","M3 4H19V18H3ZM6 8H16V14H6Z",ConsoleShell.scroll(featureControlPane));
        shell.label("Fuel Features","Combustível e proteções");
        shell.addPage("IO Functions","Funções de entradas e saídas","Escolhe as funções compatíveis pelo nome, sem códigos de pinos do microcontrolador.","M2 3H20V7H2ZM2 10H20V14H2ZM2 17H20V21H2Z",ConsoleShell.scroll(ioAssignmentsPane));
        shell.label("IO Functions","Funções de I/O");
        projectPane=new EngineProjectPane(stage,ecuAccess,saved->{
            updateMapViews(saved.project().ve());updateMapViews(saved.project().ignition());dashboard.selectKind(CalibrationMap.Kind.VE);
            configurationPane.loaded(new ConfigurationClient.Snapshot(saved.project().core(),saved.validation()));
            auxControlPane.invalidateProject();featureControlPane.invalidateProject();ioAssignmentsPane.invalidateProject();autoTunePane.invalidateProject();
            engineControlPane.externalState(saved.engine());engineArmed=saved.engine().armed();engineAutonomous=saved.engine().autonomous();refreshControls.run();
        });
        shell.addPage("ECU Project","Projeto completo da ECU","Motor, sensores, mapas e auxiliares num ficheiro, com restauração e releitura das quatro páginas.",
            "M3 1H14L20 7V21H3ZM14 1V7H20ZM6 11H16V13H6ZM6 16H16V18H6Z",ConsoleShell.scroll(projectPane));
        shell.label("ECU Project","Projeto completo");
        shell.label("Dashboard","Painel");shell.label("Data Log","Datalogs");shell.label("Tuning","Mapas e cálculos");
        shell.label("Fuel Tables","Combustível · VE");shell.label("Ignition Tables","Ignição · avanço");
        shell.label("Inputs / Outputs","Calibração sensores");shell.label("Diagnostics","Estado da ECU");
        shell.label("Engine","Motor e saídas");shell.label("Output Tests","Testes de atuadores");shell.label("Settings","Sobre a Console");
        shell.group("Monitorização","Dashboard","Data Log");
        shell.group("Configuração","Engine","Inputs / Outputs","IO Functions","Aux Control","Fuel Features");
        shell.group("Afinação","Fuel Tables","Ignition Tables","Tuning","Autotune");
        shell.group("Diagnóstico","Output Tests","Diagnostics");
        shell.group("Sistema","Firmware","Settings");
        shell.finishNavigation();
        javafx.geometry.Rectangle2D screen = javafx.stage.Screen.getPrimary().getVisualBounds();
        stage.setScene(new Scene(shell, Math.min(1536, screen.getWidth() - 40),
            Math.min(1000, screen.getHeight() - 60)));
        ConsoleBranding.applyTheme(stage.getScene());
        stage.getScene().heightProperty().addListener((property, before, height) ->
            root.setBottom(height.doubleValue() < 800 ? null : summary));
        if (stage.getScene().getHeight() < 800) root.setBottom(null);
        stage.setMinWidth(Math.min(960, screen.getWidth() - 40));
        stage.setMinHeight(Math.min(680, screen.getHeight() - 60));
        stage.setOnCloseRequest(event -> {
            if(engineArmed&&!engineAutonomous){event.consume();ConsoleBranding.alert(Alert.AlertType.INFORMATION,
                "Pára as saídas em Motor e saídas ou Testes de atuadores antes de fechar a Console.").show();return;}
            if(configurationBusy||projectPane!=null&&projectPane.isBusy()){
                event.consume();
                ConsoleBranding.alert(Alert.AlertType.INFORMATION,"Espera que a operação de configuração termine antes de fechar.").show();
                return;
            }
            if (firmwarePanel.isBusy()) {
                event.consume();
                ConsoleBranding.alert(Alert.AlertType.INFORMATION, "Espera que a operação de firmware termine antes de fechar.").show();
                return;
            }
            if(configurationPane!=null&&!configurationPane.canClose()){event.consume();return;}
            if (!CalibrationViewer.closeAll()) { event.consume(); }
        });
        stage.show();
        refreshConnection.run();
        if (!preview) scan.fire();
    }

    @Override
    public void stop() {
        if (!stopping.compareAndSet(false, true)) {
            return;
        }
        keepMonitoring.set(false);
        if (portRefresh != null) portRefresh.stop();
        if (telemetry != null) telemetry.stopPlayback();
        if(engineControlPane!=null)engineControlPane.close();
        if(projectPane!=null)projectPane.close();
        if (firmwarePanel != null) firmwarePanel.close();
        if (activeRead != null) {
            activeRead.cancel(true);
        }
        fileWorker.shutdownNow();discoveryWorker.shutdownNow();
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
