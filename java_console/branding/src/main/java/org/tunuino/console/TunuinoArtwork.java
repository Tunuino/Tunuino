package org.tunuino.console;

import javax.swing.*;
import java.awt.*;
import java.awt.image.BufferedImage;

/** Swing-compatible Tunuino blue-and-white vector brand artwork. */
public final class TunuinoArtwork {
    public static final Color BLUE = new Color(0x119AF5);
    public static final Color WHITE = new Color(0xF4F7FC);
    public static final Color BACKGROUND = new Color(0x151C25);

    private TunuinoArtwork() { }

    public static ImageIcon icon(int size) {
        if (size < 16) throw new IllegalArgumentException("size must be at least 16");
        BufferedImage image = new BufferedImage(size, size, BufferedImage.TYPE_INT_ARGB);
        Graphics2D g = image.createGraphics();
        try {
            g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
            g.scale(size / 100.0, size / 100.0);
            Polygon blue = new Polygon(new int[] {5, 95, 83, 61, 50, 39, 17}, new int[] {12, 12, 32, 32, 94, 70, 32}, 7);
            g.setColor(BLUE);
            g.fillPolygon(blue);
            g.setColor(WHITE);
            g.fillPolygon(new int[] {27, 73, 63, 56, 44, 37}, new int[] {26, 26, 38, 38, 68, 38}, 6);
        } finally {
            g.dispose();
        }
        return new ImageIcon(image);
    }

    public static JPanel wordmark() {
        JPanel panel = new JPanel(new BorderLayout(14, 0));
        panel.setOpaque(false);
        JLabel symbol = new JLabel(icon(70));
        JLabel name = new JLabel("TUNUINO");
        name.setFont(new Font(Font.SANS_SERIF, Font.BOLD, 34));
        name.setForeground(WHITE);
        panel.add(symbol, BorderLayout.WEST);
        panel.add(name, BorderLayout.CENTER);
        return panel;
    }
}
