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
        if (args.length == 1 && args[0].equals("--verify-runtime")) {
            Platform.startup(() -> Platform.runLater(() -> {
                try {
                    Stage stage = new Stage();
                    ConsoleBranding.applyIcon(stage);
                    // Construct the actual firmware controls as well as the graphics runtime.
                    FirmwarePanel firmware = new FirmwarePanel(stage, () -> true, () -> null, () -> true);
                    stage.setScene(new Scene(firmware, 1000, 360));
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
