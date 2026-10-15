package org.tunuino.ui;

import org.tunuino.console.ConsoleWindowBranding;
import javax.swing.*;
import java.awt.*;

/** Standalone desktop status viewer; sample values are local, not ECU data. */
public final class ConsoleStatusApp {
    private ConsoleStatusApp() { }

    public static JPanel createContent() {
        JPanel root = new JPanel(new BorderLayout());
        root.setBackground(new Color(20, 27, 40));
        root.add(ConsoleWindowBranding.createHeader(), BorderLayout.NORTH);
        root.add(new ConsoleStatusPanel(), BorderLayout.CENTER);
        JLabel connection = new JLabel("ECU disconnected - waiting for data");
        connection.setForeground(new Color(185, 205, 230));
        connection.setBorder(BorderFactory.createEmptyBorder(8, 16, 12, 16));
        root.add(connection, BorderLayout.SOUTH);
        return root;
    }

    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> {
            JFrame frame = new JFrame();
            ConsoleWindowBranding.apply(frame);
            frame.setDefaultCloseOperation(WindowConstants.EXIT_ON_CLOSE);
            frame.setContentPane(createContent());
            frame.setMinimumSize(new Dimension(480, 280));
            frame.pack();
            frame.setLocationRelativeTo(null);
            frame.setVisible(true);
        });
    }
}
