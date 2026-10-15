package org.tunuino.ui;

import javax.swing.SwingUtilities;
import java.util.concurrent.atomic.AtomicReference;

public final class ConsoleStatusPanelSmokeTest {
    public static void main(String[] args) throws Exception {
        AtomicReference<Throwable> failure = new AtomicReference<>();
        SwingUtilities.invokeAndWait(() -> {
            try {
                ConsoleStatusPanel panel = new ConsoleStatusPanel();
                if (!"-- rpm".equals(panel.getReadingText("RPM"))) throw new AssertionError("initial state");
                panel.setReading("RPM", 850, "rpm");
                if (!"850.0 rpm".equals(panel.getReadingText("RPM"))) throw new AssertionError("update");
                try {
                    panel.setReading("RPM", Double.NaN, "rpm");
                    throw new AssertionError("invalid reading accepted");
                } catch (IllegalArgumentException expected) { }
            } catch (Throwable error) { failure.set(error); }
        });
        if (failure.get() != null) throw new AssertionError(failure.get());
    }
}
