package org.tunuino.console;

import javax.swing.*;
import java.awt.*;

/** Standalone layout preview with placeholder data, not a connected ECU Console. */
public final class BrandingPreview {
    private static final Color PANEL = new Color(0x1B2735);
    private static final Color TEXT = new Color(0xD9E6F5);

    private BrandingPreview() { }

    private static JPanel tile(String label, String value) {
        JPanel tile = new JPanel(new BorderLayout(0, 8));
        tile.setBackground(PANEL);
        tile.setBorder(BorderFactory.createEmptyBorder(18, 20, 18, 20));
        JLabel heading = new JLabel(label);
        heading.setForeground(TunuinoArtwork.BLUE);
        JLabel reading = new JLabel(value);
        reading.setFont(new Font(Font.SANS_SERIF, Font.BOLD, 28));
        reading.setForeground(TunuinoArtwork.WHITE);
        tile.add(heading, BorderLayout.NORTH);
        tile.add(reading, BorderLayout.CENTER);
        return tile;
    }

    private static JPanel dashboard() {
        JPanel content = new JPanel(new BorderLayout(12, 16));
        content.setBackground(TunuinoArtwork.BACKGROUND);
        content.setBorder(BorderFactory.createEmptyBorder(18, 22, 22, 22));
        content.add(ConsoleWindowBranding.createHeader(), BorderLayout.NORTH);

        JPanel middle = new JPanel(new BorderLayout(12, 12));
        middle.setOpaque(false);
        JPanel navigation = new JPanel(new GridLayout(0, 1, 0, 10));
        navigation.setOpaque(false);
        for (String item : new String[] {"Dashboard", "Tuning", "Fuel Tables",
                "Ignition Tables", "Sensors", "Diagnostics", "Datalogger"}) {
            JLabel entry = new JLabel(item);
            entry.setForeground(TEXT);
            entry.setBorder(BorderFactory.createEmptyBorder(10, 12, 10, 25));
            navigation.add(entry);
        }
        middle.add(navigation, BorderLayout.WEST);

        JPanel instruments = new JPanel(new GridLayout(2, 3, 12, 12));
        instruments.setOpaque(false);
        instruments.add(tile("RPM", "3580"));
        instruments.add(tile("MAP", "98 kPa"));
        instruments.add(tile("TPS", "32 %"));
        instruments.add(tile("AFR", "14.7"));
        instruments.add(tile("Coolant", "92 °C"));
        instruments.add(tile("Intake", "36 °C"));
        middle.add(instruments, BorderLayout.CENTER);
        content.add(middle, BorderLayout.CENTER);

        JLabel notice = new JLabel("DEMO PREVIEW  |  Simulated values  |  ECU not connected");
        notice.setForeground(TunuinoArtwork.BLUE);
        content.add(notice, BorderLayout.SOUTH);
        return content;
    }

    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> {
            JFrame frame = new JFrame();
            ConsoleWindowBranding.apply(frame);
            frame.setTitle(ConsoleBranding.APPLICATION_NAME + " | Dashboard preview");
            frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
            frame.setContentPane(dashboard());
            frame.setSize(1080, 650);
            frame.setMinimumSize(new Dimension(780, 480));
            frame.setLocationRelativeTo(null);
            frame.setVisible(true);
        });
    }
}
