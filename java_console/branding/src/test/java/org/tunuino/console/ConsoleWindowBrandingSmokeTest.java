package org.tunuino.console;

import javax.swing.*;
import java.awt.*;

public final class ConsoleWindowBrandingSmokeTest {
    public static void main(String[] args) {
        JPanel header = ConsoleWindowBranding.createHeader();
        if (!TunuinoArtwork.BACKGROUND.equals(header.getBackground())) {
            throw new AssertionError("Incorrect header background");
        }
        if (!(header.getLayout() instanceof BorderLayout)) {
            throw new AssertionError("Incorrect header layout");
        }
        if (header.getComponentCount() != 1) {
            throw new AssertionError("Missing Tunuino wordmark");
        }
    }
}
