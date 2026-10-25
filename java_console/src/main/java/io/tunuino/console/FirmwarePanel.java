/* Copyright (c) 2026 Tunuino. */
package io.tunuino.console;

import io.tunuino.maintenance.*;
import javafx.application.Platform;
import javafx.geometry.Insets;
import javafx.scene.control.*;
import javafx.scene.layout.VBox;
import javafx.stage.FileChooser;
import javafx.stage.Stage;
import java.io.*;
import java.nio.file.Path;
import java.time.Duration;
import java.util.*;
import java.util.concurrent.*;
import java.util.function.*;

/** Firmware operations are serialized with device discovery and never run on the FX thread. */
final class FirmwarePanel extends VBox implements AutoCloseable {
    private final ScheduledExecutorService worker = Executors.newSingleThreadScheduledExecutor(r -> {
        Thread t = new Thread(r, "tunuino-firmware"); t.setDaemon(true); return t;
    });
    private final ProgrammerTools tools = new ProgrammerTools();
    private final BooleanSupplier serialFree;
    private final Supplier<String> comPort;
    private final BooleanSupplier proteusSelected;
    private final Stage stage;
    private final Label state = new Label("A pesquisar dispositivos DFU…");
    private final Label imageInfo = new Label("Seleciona firmware .bin/.srec com o respetivo manifesto .properties");
    private final ComboBox<WindowsDfu.UsbPort> usbPorts = new ComboBox<>();
    private final Button flashDfu = new Button("Gravar por DFU");
    private final Button flashBlt = new Button("Gravar por OpenBLT");
    private final TextArea log = new TextArea();
    private final List<Button> setupButtons = new ArrayList<>();
    private final java.util.concurrent.atomic.AtomicInteger pendingLog = new java.util.concurrent.atomic.AtomicInteger();
    private volatile boolean busy;
    private volatile boolean closed;
    private boolean exclusive;
    private FirmwareImage image;
    private Path stTool;
    private Path bltTool;
    private String lastDiscovery = "";
    private final boolean discoveryEnabled;

    FirmwarePanel(Stage stage, BooleanSupplier serialFree, Supplier<String> comPort, BooleanSupplier proteusSelected) {
        this(stage, serialFree, comPort, proteusSelected, true);
    }

    FirmwarePanel(Stage stage, BooleanSupplier serialFree, Supplier<String> comPort,
            BooleanSupplier proteusSelected, boolean discoveryEnabled) {
        this.stage = stage; this.serialFree = serialFree; this.comPort = comPort; this.proteusSelected = proteusSelected;
        this.discoveryEnabled = discoveryEnabled;
        setPadding(new Insets(0, 24, 24, 24)); setSpacing(14);
        stTool = tools.find("STM32_Programmer_CLI.exe");
        bltTool = tools.find("BootCommander.exe");
        Button selectImage = button("Selecionar firmware", this::chooseImage);
        Button chooseSt = button("Ferramenta ST…", () -> chooseTool("STM32_Programmer_CLI.exe"));
        Button chooseBlt = button("Ferramenta OpenBLT…", () -> chooseTool("BootCommander.exe"));
        Button drivers = button("Instalar drivers…", this::installDrivers);
        Button refresh = button("Pesquisar DFU", this::scan);
        flashDfu.setOnAction(e -> flash(false)); flashBlt.setOnAction(e -> flash(true));
        log.setEditable(false); log.setPrefRowCount(6); log.setWrapText(true); log.setMinHeight(120);
        log.setPromptText("O registo da ferramenta de gravação aparece aqui.");
        VBox.setVgrow(log, javafx.scene.layout.Priority.ALWAYS);
        state.setWrapText(true); imageInfo.setWrapText(true);
        selectImage.getStyleClass().add("primary"); flashDfu.getStyleClass().add("primary");
        TitledPane advanced = new TitledPane("Ferramentas e drivers", ConsoleShell.actions(chooseSt, chooseBlt, drivers));
        advanced.setExpanded(false);
        VBox imageCard = new VBox(12, ConsoleShell.caption("01  IMAGEM DE FIRMWARE"), imageInfo, selectImage);
        imageCard.getStyleClass().add("panel");
        VBox deviceCard = new VBox(12, ConsoleShell.caption("02  DISPOSITIVO USB"),
            ConsoleShell.actions(usbPorts, refresh), state, ConsoleShell.actions(flashDfu, flashBlt));
        deviceCard.getStyleClass().add("panel");
        getChildren().addAll(imageCard, deviceCard, advanced, ConsoleShell.caption("REGISTO DA OPERAÇÃO"), log);
        usbPorts.setPromptText("Dispositivo USB DFU"); usbPorts.setPrefWidth(320);
        usbPorts.setOnAction(e -> refreshControls());
        javafx.animation.Timeline timer = new javafx.animation.Timeline(
            new javafx.animation.KeyFrame(javafx.util.Duration.seconds(3), e -> scan()));
        timer.setCycleCount(javafx.animation.Animation.INDEFINITE);
        if (discoveryEnabled) timer.play();
        else state.setText("Pré-visualização da interface · pesquisa de dispositivos desativada");
        discoveryTimer = timer;
        refreshControls(); scan();
    }
    private final javafx.animation.Timeline discoveryTimer;
    private Button button(String title, Runnable action) {
        Button button = new Button(title); button.setOnAction(e -> action.run()); setupButtons.add(button); return button;
    }
    void refreshControls() {
        boolean available = discoveryEnabled && !busy && serialFree.getAsBoolean() && proteusSelected.getAsBoolean();
        flashDfu.setDisable(!available || image == null || !image.mode().equals("dfu") || stTool == null || usbPorts.getValue() == null);
        flashBlt.setDisable(!available || image == null || !image.mode().equals("openblt") || bltTool == null || comPort.get() == null);
        usbPorts.setDisable(busy);
        setupButtons.forEach(button -> button.setDisable(busy || !discoveryEnabled));
    }
    private void append(String text) {
        // Keep process output bounded even if a tool floods stdout while the UI is busy.
        if (pendingLog.incrementAndGet() > 100) { pendingLog.decrementAndGet(); return; }
        Platform.runLater(() -> {
            pendingLog.decrementAndGet();
            if (closed) return;
            log.appendText(text);
            if (log.getLength() > 48000) log.deleteText(0, log.getLength() - 48000);
        });
    }
    private <T> void execute(Callable<T> operation, Consumer<T> complete) {
        execute(operation, complete, false);
    }
    private <T> void execute(Callable<T> operation, Consumer<T> complete, boolean protect) {
        if (busy || closed) return;
        busy = true; exclusive = protect; refreshControls();
        worker.submit(() -> {
            try {
                T value = operation.call();
                Platform.runLater(() -> { if (!closed) { busy = false; exclusive = false; complete.accept(value); refreshControls(); } });
            } catch (Exception e) {
                Platform.runLater(() -> {
                    if (!closed) { busy = false; exclusive = false; state.setText("Erro: " + e.getMessage()); append("\n" + e + "\n"); refreshControls(); }
                });
            }
        });
    }
    private record Discovery(List<WindowsDfu.Device> devices, List<WindowsDfu.UsbPort> ports, String problem) {}
    private void scan() {
        if (busy || closed || !discoveryEnabled) return;
        Path tool = stTool;
        execute(() -> {
            List<WindowsDfu.Device> devices = WindowsDfu.scan();
            if (devices.isEmpty() || tool == null || devices.stream().noneMatch(d -> d.driverError() == 0))
                return new Discovery(devices, List.of(), "");
            try { return new Discovery(devices, WindowsDfu.listPorts(tool), ""); }
            catch (IOException e) { return new Discovery(devices, List.of(), e.getMessage()); }
        }, result -> {
            WindowsDfu.UsbPort previous = usbPorts.getValue();
            usbPorts.getItems().setAll(result.ports());
            if (previous != null) result.ports().stream().filter(p -> p.serial().equals(previous.serial())).findFirst().ifPresent(usbPorts::setValue);
            if (usbPorts.getValue() == null && result.ports().size() == 1) usbPorts.setValue(result.ports().get(0));
            String description;
            if (result.devices().isEmpty()) description = "DFU não encontrado. Em arranque normal a ECU aparece nas portas COM; para DFU usa BOOT0 e reset.";
            else if (result.devices().stream().anyMatch(d -> d.driverError() != 0)) description = "USB DFU detetado com erro de driver. Usa Instalar drivers. " + result.devices();
            else if (tool == null) description = "USB DFU detetado. Seleciona STM32_Programmer_CLI.exe da instalação STM32CubeProgrammer.";
            else if (!result.problem().isEmpty()) description = "USB DFU detetado. " + result.problem();
            else description = "DFU: " + result.ports().size() + " dispositivo(s) disponível(is) na ferramenta ST. Desliga a sessão COM antes de gravar.";
            if (!description.equals(lastDiscovery)) { state.setText(description); lastDiscovery = description; }
        });
    }
    private File chooseExe(String title, String name) {
        FileChooser chooser = new FileChooser(); chooser.setTitle(title);
        chooser.getExtensionFilters().add(new FileChooser.ExtensionFilter(name, "*.exe"));
        return chooser.showOpenDialog(stage);
    }
    private void chooseTool(String name) {
        File selected = chooseExe("Selecionar " + name + " (manter DLLs e pastas da ferramenta)", name);
        if (selected == null) return;
        try {
            tools.select(name, selected.toPath());
            if (name.equals("STM32_Programmer_CLI.exe")) stTool = selected.toPath().toAbsolutePath();
            else bltTool = selected.toPath().toAbsolutePath();
            state.setText("Ferramenta configurada: " + selected); lastDiscovery = ""; refreshControls(); scan();
        } catch (IOException e) { state.setText(e.getMessage()); }
    }
    private void chooseImage() {
        FileChooser chooser = new FileChooser(); chooser.setTitle("Firmware para Proteus v0.7 STM32F427");
        chooser.getExtensionFilters().add(new FileChooser.ExtensionFilter("Firmware com manifesto", "*.bin", "*.srec"));
        File selected = chooser.showOpenDialog(stage);
        if (selected == null) return;
        image = null;
        imageInfo.setText("A validar " + selected.getName() + "…");
        execute(() -> FirmwareImage.load(selected.toPath()), loaded -> {
            image = loaded;
            imageInfo.setText(loaded.description() + " · " + loaded.mode().toUpperCase(Locale.ROOT) + " · 0x"
                + Long.toHexString(loaded.address()) + (loaded.replacesBootloader() ? " · SUBSTITUI O BOOTLOADER EXISTENTE" : " · preserva a área do bootloader"));
            append("\nFirmware validado: " + loaded.file() + "\nSHA-256: " + loaded.sha256() + "\n");
        });
    }
    private void installDrivers() {
        Path installer = tools.find("silent_st_drivers2.exe");
        if (installer == null) {
            File selected = chooseExe("Selecionar instalador dos drivers ST", "silent_st_drivers2.exe");
            if (selected == null) return;
            installer = selected.toPath().toAbsolutePath();
            try { tools.select("silent_st_drivers2.exe", installer); }
            catch (IOException e) { state.setText(e.getMessage()); return; }
        }
        Alert prompt = ConsoleBranding.alert(Alert.AlertType.CONFIRMATION,
            "Instalar os drivers ST deste ficheiro?\n" + installer + "\nO Windows pode pedir autorização de administrador.", ButtonType.OK, ButtonType.CANCEL);
        prompt.initOwner(stage);
        if (prompt.showAndWait().orElse(ButtonType.CANCEL) != ButtonType.OK) return;
        Path source = installer;
        // Copy to a writable directory: the self-extractor unpacks beside its executable.
        execute(() -> {
            Path folder = java.nio.file.Files.createTempDirectory("tunuino-st-drivers-");
            try {
                Path archive = folder.resolve("silent_st_drivers2.exe");
                java.nio.file.Files.copy(source, archive);
                CommandRunner.Result unpack = CommandRunner.run(List.of(archive.toString(), "-y"), folder, Duration.ofMinutes(2), this::append);
                if (unpack.exitCode() != 0) throw new IOException("Falha ao extrair os drivers");
                Path batch = folder.resolve("silent_st_drivers/silent_install_windows8.bat");
                if (!java.nio.file.Files.isRegularFile(batch)) throw new IOException("Instalador sem o script de instalação esperado");
                CommandRunner.Result install = CommandRunner.run(List.of("cmd.exe", "/d", "/c", "silent_install_windows8.bat"),
                    batch.getParent(), Duration.ofMinutes(3), this::append);
                if (install.exitCode() != 0) throw new IOException("Instalação dos drivers terminou com erro. Consulta o registo.");
                return "Instalador dos drivers terminou. A deteção DFU vai verificar o estado do dispositivo.";
            } finally {
                try (var files = java.nio.file.Files.walk(folder)) {
                    for (Path file : files.sorted(Comparator.reverseOrder()).toList()) java.nio.file.Files.deleteIfExists(file);
                }
            }
        }, state::setText, true);
    }
    private void flash(boolean openblt) {
        if (busy || image == null || !serialFree.getAsBoolean() || !proteusSelected.getAsBoolean()) return;
        FirmwareImage selected = image;
        WindowsDfu.UsbPort dfu = usbPorts.getValue();
        String port = comPort.get();
        Path tool = openblt ? bltTool : stTool;
        if (tool == null || (openblt ? port == null : dfu == null) || !selected.mode().equals(openblt ? "openblt" : "dfu")) return;
        String warning = "Confirmas que a placa é Proteus v0.7 com STM32F427?\n\n" + selected.description()
            + "\nDestino: " + (openblt ? port : dfu) + "\n\n";
        warning += selected.replacesBootloader()
            ? "Esta imagem experimental SUBSTITUI o bootloader na flash. Guarda primeiro a instalação atual. Não controla o motor.\n"
            : "A ECU tem de estar no bootloader OpenBLT. Não existe ainda mudança automática de modo.\n";
        warning += "Faz o ensaio na bancada, com bobinas, injetores e restantes atuadores desligados.";
        Alert confirm = ConsoleBranding.alert(Alert.AlertType.CONFIRMATION, warning, ButtonType.OK, ButtonType.CANCEL);
        confirm.initOwner(stage); confirm.setHeaderText("Gravar firmware por " + (openblt ? "OpenBLT" : "DFU"));
        if (confirm.showAndWait().orElse(ButtonType.CANCEL) != ButtonType.OK) return;
        if (!serialFree.getAsBoolean()) { state.setText("Desliga a sessão COM antes de gravar"); return; }
        log.clear(); state.setText("Gravação em curso. Não desligues a alimentação nem feches a Console.");
        append("Tunuino Console · " + BuildInfo.label() + "\nFerramenta: " + tool
            + "\nFirmware: " + selected.file() + "\n");
        execute(() -> {
            if (openblt) OpenBltProgrammer.flash(tool, port, selected, this::append);
            else WindowsDfu.flash(tool, dfu, selected, this::append);
            return openblt ? "OpenBLT terminou sem erro; volta a identificar a ECU." : "Gravação DFU verificada. Repõe BOOT0 e reinicia a ECU.";
        }, state::setText, true);
    }
    boolean isBusy() { return exclusive; }
    @Override public void close() { closed = true; discoveryTimer.stop(); worker.shutdownNow(); }
}
