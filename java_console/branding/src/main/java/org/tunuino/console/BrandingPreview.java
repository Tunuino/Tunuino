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

    private static JPanel demoMap(String title, int offset) {
        JPanel panel = new JPanel(new BorderLayout(0, 10));
        panel.setBackground(PANEL);
        panel.setBorder(BorderFactory.createEmptyBorder(12, 14, 14, 14));
        JLabel label = new JLabel(title + " (demo)");
        label.setForeground(TunuinoArtwork.WHITE);
        label.setFont(new Font(Font.SANS_SERIF, Font.BOLD, 15));
        panel.add(label, BorderLayout.NORTH);

        String[] columns = {"kPa / RPM", "1000", "2000", "3000", "4000", "5000"};
        Object[][] values = new Object[5][6];
        for (int row = 0; row < 5; row++) {
            values[row][0] = Integer.toString(20 + row * 30);
            for (int column = 1; column < 6; column++) {
                values[row][column] = Integer.toString(offset + row * 4 + column * 3);
            }
        }
        JTable table = new JTable(values, columns);
        table.setEnabled(false);
        table.setRowHeight(27);
        table.setBackground(new Color(0x223449));
        table.setForeground(TEXT);
        table.setGridColor(new Color(0x3A5269));
        table.getTableHeader().setBackground(new Color(0x145587));
        table.getTableHeader().setForeground(Color.WHITE);
        JScrollPane scroll = new JScrollPane(table);
        scroll.setBorder(BorderFactory.createEmptyBorder());
        panel.add(scroll, BorderLayout.CENTER);
        return panel;
    }

    private static JPanel demoGraph() {
        JPanel panel = new JPanel(new BorderLayout(0, 8));
        panel.setBackground(PANEL);
        panel.setBorder(BorderFactory.createEmptyBorder(12, 14, 12, 14));
        JLabel label = new JLabel("Realtime Graphs (demo)");
        label.setForeground(TunuinoArtwork.WHITE);
        panel.add(label, BorderLayout.NORTH);
        JPanel plot = new JPanel() {
            @Override
            protected void paintComponent(Graphics graphics) {
                super.paintComponent(graphics);
                Graphics2D g = (Graphics2D) graphics.create();
                try {
                    g.setRenderingHint(RenderingHints.KEY_ANTIALIASING,
                            RenderingHints.VALUE_ANTIALIAS_ON);
                    int width = getWidth();
                    int height = getHeight();
                    g.setColor(new Color(0x33475C));
                    for (int i = 1; i < 5; i++) {
                        int y = i * height / 5;
                        g.drawLine(0, y, width, y);
                    }
                    Color[] colors = {TunuinoArtwork.BLUE, new Color(0xFF6C6C),
                            new Color(0x60D995)};
                    for (int series = 0; series < colors.length; series++) {
                        g.setColor(colors[series]);
                        int lastX = 0;
                        int lastY = height / 2;
                        for (int x = 0; x < width; x += 4) {
                            double phase = x * 0.022 + series * 1.7;
                            int y = (int) (height * (0.45 + series * 0.11)
                                    - Math.sin(phase) * height * 0.12
                                    - Math.sin(phase * 0.41) * height * 0.07);
                            if (x != 0) g.drawLine(lastX, lastY, x, y);
                            lastX = x;
                            lastY = y;
                        }
                    }
                } finally {
                    g.dispose();
                }
            }
        };
        plot.setBackground(new Color(0x111B27));
        plot.setPreferredSize(new Dimension(200, 115));
        panel.add(plot, BorderLayout.CENTER);
        return panel;
    }

    private static JPanel dashboard() {
        JPanel content = new JPanel(new BorderLayout(12, 16));
        content.setBackground(TunuinoArtwork.BACKGROUND);
        content.setBorder(BorderFactory.createEmptyBorder(18, 22, 22, 22));
        JPanel top = new JPanel(new BorderLayout(12, 0));
        top.setOpaque(false);
        top.add(ConsoleWindowBranding.createHeader(), BorderLayout.CENTER);
        JLabel connection = new JLabel("ECU DISCONNECTED  |  DEMO MODE");
        connection.setForeground(new Color(0xFFBE65));
        connection.setFont(new Font(Font.SANS_SERIF, Font.BOLD, 13));
        connection.setBorder(BorderFactory.createEmptyBorder(0, 12, 0, 12));
        top.add(connection, BorderLayout.EAST);
        content.add(top, BorderLayout.NORTH);

        JPanel middle = new JPanel(new BorderLayout(12, 12));
        middle.setOpaque(false);
        JPanel navigation = new JPanel(new GridLayout(0, 1, 0, 10));
        navigation.setOpaque(false);
        JPanel pages = new JPanel(new CardLayout());
        pages.setOpaque(false);
        String[] sections = {"Dashboard", "Tuning", "Fuel Tables",
                "Ignition Tables", "Sensors", "Diagnostics", "Datalogger"};
        for (String item : sections) {
            JButton entry = new JButton(item);
            entry.setHorizontalAlignment(SwingConstants.LEFT);
            entry.setForeground(TEXT);
            entry.setBackground(PANEL);
            entry.setFocusPainted(false);
            entry.setBorder(BorderFactory.createEmptyBorder(10, 12, 10, 25));
            entry.addActionListener(event ->
                    ((CardLayout) pages.getLayout()).show(pages, item));
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
        JPanel mainArea = new JPanel(new BorderLayout(12, 12));
        mainArea.setOpaque(false);
        mainArea.add(instruments, BorderLayout.NORTH);
        JPanel maps = new JPanel(new GridLayout(1, 2, 12, 0));
        maps.setOpaque(false);
        maps.add(demoMap("Fuel VE Table", 30));
        maps.add(demoMap("Ignition Advance", 8));
        mainArea.add(maps, BorderLayout.CENTER);
        mainArea.add(demoGraph(), BorderLayout.SOUTH);
        pages.add(mainArea, "Dashboard");
        for (String item : sections) {
            if ("Dashboard".equals(item)) continue;
            JPanel placeholder = new JPanel(new BorderLayout());
            placeholder.setBackground(PANEL);
            JLabel title = new JLabel(item + "  |  Preview only", SwingConstants.CENTER);
            title.setForeground(TunuinoArtwork.WHITE);
            title.setFont(new Font(Font.SANS_SERIF, Font.BOLD, 24));
            placeholder.add(title, BorderLayout.CENTER);
            pages.add(placeholder, item);
        }
        middle.add(pages, BorderLayout.CENTER);
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
            frame.setSize(1080, 780);
            frame.setMinimumSize(new Dimension(780, 480));
            frame.setLocationRelativeTo(null);
            frame.setVisible(true);
        });
    }
}
