package org.tunuino.console;

import javax.swing.*;
import java.awt.*;

/** Standalone visual preview, not a replacement for the upstream ECU Console. */
public final class BrandingPreview {
    private BrandingPreview() { }

    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> {
            JFrame frame = new JFrame();
            ConsoleWindowBranding.apply(frame);
            frame.setTitle(ConsoleBranding.APPLICATION_NAME + " | Branding preview");
            frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
            JPanel content = new JPanel(new BorderLayout(0, 20));
            content.setBackground(TunuinoArtwork.BACKGROUND);
            content.setBorder(BorderFactory.createEmptyBorder(20, 20, 20, 20));
            content.add(ConsoleWindowBranding.createHeader(), BorderLayout.CENTER);
            JLabel caption = new JLabel("ENGINE MANAGEMENT  |  Console branding preview");
            caption.setForeground(TunuinoArtwork.BLUE);
            content.add(caption, BorderLayout.SOUTH);
            frame.setContentPane(content);
            frame.setSize(620, 230);
            frame.setLocationRelativeTo(null);
            frame.setVisible(true);
        });
    }
}
