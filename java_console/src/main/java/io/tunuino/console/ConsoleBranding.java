/* Copyright (c) 2026 Tunuino. */
package io.tunuino.console;

import java.util.Objects;
import javafx.scene.image.Image;
import javafx.scene.Scene;
import javafx.stage.Stage;
import javafx.scene.control.Alert;
import javafx.scene.control.ButtonType;

/** Shared identity for the Console and calibration windows. */
final class ConsoleBranding {
    private ConsoleBranding() {}

    static void applyIcon(Stage stage) {
        var resource = Objects.requireNonNull(
                ConsoleBranding.class.getResource("tunuino.png"),
                "Missing Tunuino logo resource");
        Image icon = new Image(resource.toExternalForm(), 256, 256, true, true);
        if (icon.isError()) {
            throw new IllegalStateException("Cannot load Tunuino logo", icon.getException());
        }
        stage.getIcons().setAll(icon);
    }

    static void applyTheme(Scene scene) {
        String stylesheet = Objects.requireNonNull(ConsoleBranding.class.getResource("console.css"),
            "Missing Console stylesheet").toExternalForm();
        if (!scene.getStylesheets().contains(stylesheet)) scene.getStylesheets().add(stylesheet);
    }

    static Alert alert(Alert.AlertType type, String content, ButtonType... buttons) {
        Alert alert = new Alert(type, content, buttons);
        alert.getDialogPane().getStylesheets().add(ConsoleBranding.class.getResource("console.css").toExternalForm());
        return alert;
    }
}
