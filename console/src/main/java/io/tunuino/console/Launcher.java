/* Copyright (c) 2026 Tunuino. */
package io.tunuino.console;

import io.tunuino.transport.UsbSerialPorts;
import javafx.application.Platform;
import javafx.scene.Scene;
import javafx.scene.control.Label;
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
                    stage.setScene(new Scene(new Label("Tunuino runtime check"),320,100));
                    stage.show();
                    UsbSerialPorts.list();
                    stage.close();
                    System.out.println("JavaFX graphics, controls, logo and serial enumeration loaded");
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
