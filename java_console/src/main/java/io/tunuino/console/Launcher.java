/* Copyright (c) 2026 Tunuino. */
package io.tunuino.console;

import io.tunuino.transport.UsbSerialPorts;
import io.tunuino.maintenance.WindowsDfu;
import javafx.application.Platform;
import javafx.scene.Scene;
import javafx.stage.Stage;

/** Classpath entry point for the Windows application image. */
public final class Launcher {
    private Launcher() {}

    public static void main(String[] args) {
        if (args.length == 1 && (args[0].equals("--verify-runtime") || args[0].equals("--verify-installation"))) {
            Platform.startup(() -> Platform.runLater(() -> {
                try {
                    if (args[0].equals("--verify-installation")) {
                        java.nio.file.Path folder = io.tunuino.maintenance.AppPaths.firmware();
                        if (folder == null) throw new java.io.IOException("Bundled firmware not found from installed application");
                        try (var files = java.nio.file.Files.list(folder)) {
                            var image = files.filter(p -> p.toString().endsWith(".bin")).findFirst().orElseThrow();
                            io.tunuino.maintenance.FirmwareImage.load(image);
                        }
                        System.out.println("Bundled firmware validated: " + folder);
                    }
                    Stage stage = new Stage();
                    ConsoleBranding.applyIcon(stage);
                    // Construct the actual firmware controls as well as the graphics runtime.
                    FirmwarePanel firmware = new FirmwarePanel(stage, () -> true, () -> null, () -> true);
                    stage.setScene(new Scene(firmware, 1000, 360));
                    ConsoleBranding.applyTheme(stage.getScene());
                    stage.show();
                    UsbSerialPorts.list();
                    WindowsDfu.scan();
                    firmware.close();
                    stage.close();
                    System.out.println("JavaFX firmware controls, logo, serial enumeration and Windows DFU query loaded");
                    System.exit(0);
                } catch (Throwable error) {
                    error.printStackTrace();
                    System.exit(1);
                }
            }));
        } else {
            TunuinoConsole.main(args);
        }
    }
}
