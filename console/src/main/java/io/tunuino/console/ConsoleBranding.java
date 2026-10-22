/* Copyright (c) 2026 Tunuino. */
package io.tunuino.console;

import java.util.Objects;
import javafx.scene.image.Image;
import javafx.stage.Stage;

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
}
