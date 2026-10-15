package org.tunuino.ui.protocol;

import org.tunuino.ui.ConsoleStatusPanel;
import javax.swing.SwingUtilities;
import java.util.Objects;

/** Connects verified telemetry packets to the Swing status panel. */
public final class TelemetryPresenter {
    private final ConsoleStatusPanel panel;

    public TelemetryPresenter(ConsoleStatusPanel panel) {
        this.panel = Objects.requireNonNull(panel, "panel");
    }

    /** Accepts a complete binary telemetry frame from any transport. */
    public void accept(byte[] packet) {
        TelemetryFrame.Reading reading = TelemetryFrame.decode(packet);
        Runnable update = () -> {
            panel.setReading("RPM", reading.rpm, "rpm");
            panel.setReading("MAP", reading.mapKpa, "kPa");
            panel.setReading("Coolant", reading.coolantC, "°C");
            panel.setReading("Battery", reading.batteryV, "V");
        };
        if (SwingUtilities.isEventDispatchThread()) update.run();
        else SwingUtilities.invokeLater(update);
    }
}
