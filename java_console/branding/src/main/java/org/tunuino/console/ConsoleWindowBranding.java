package org.tunuino.console;

import javax.swing.*;
import java.awt.*;

/** Shared Swing frame and header styling for migrated Console windows. */
public final class ConsoleWindowBranding {
    private ConsoleWindowBranding() { }

    public static void apply(JFrame frame) {
        if (frame == null) throw new IllegalArgumentException("frame");
        frame.setTitle(ConsoleBranding.APPLICATION_NAME);
        frame.setIconImage(TunuinoArtwork.icon(64).getImage());
    }

    public static JPanel createHeader() {
        JPanel header = new JPanel(new BorderLayout());
        header.setBackground(TunuinoArtwork.BACKGROUND);
        header.setBorder(BorderFactory.createEmptyBorder(12, 16, 12, 16));
        header.add(TunuinoArtwork.wordmark(), BorderLayout.WEST);
        return header;
    }
}
