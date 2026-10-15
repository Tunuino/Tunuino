package org.tunuino.ui;

import javax.swing.*;
import java.awt.*;
import java.util.LinkedHashMap;
import java.util.Map;

/** Live-value display component for the Tunuino desktop console. */
public final class ConsoleStatusPanel extends JPanel {
    private final Map<String, JLabel> values = new LinkedHashMap<>();

    public ConsoleStatusPanel() {
        super(new GridLayout(0, 2, 12, 8));
        setBackground(new Color(20, 27, 40));
        setBorder(BorderFactory.createEmptyBorder(16, 16, 16, 16));
        addReading("RPM", "rpm");
        addReading("Coolant", "°C");
        addReading("Battery", "V");
        addReading("MAP", "kPa");
    }

    private void addReading(String name, String unit) {
        JLabel title = new JLabel(name);
        title.setForeground(new Color(185, 205, 230));
        JLabel value = new JLabel("-- " + unit);
        value.setForeground(Color.WHITE);
        value.setFont(value.getFont().deriveFont(Font.BOLD, 18f));
        values.put(name, value);
        add(title);
        add(value);
    }

    /** Updates a displayed reading; must be called on the Swing event thread. */
    public void setReading(String name, double reading, String unit) {
        if (!SwingUtilities.isEventDispatchThread()) {
            throw new IllegalStateException("Update readings on the Swing event thread");
        }
        JLabel value = values.get(name);
        if (value == null) throw new IllegalArgumentException("Unknown reading: " + name);
        if (!Double.isFinite(reading)) throw new IllegalArgumentException("Reading must be finite");
        value.setText(String.format(java.util.Locale.ROOT, "%.1f %s", reading, unit));
    }

    public String getReadingText(String name) {
        JLabel value = values.get(name);
        if (value == null) throw new IllegalArgumentException("Unknown reading: " + name);
        return value.getText();
    }
}
