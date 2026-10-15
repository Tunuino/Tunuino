package org.tunuino.ui;

import javax.swing.*;
import java.awt.*;
import java.util.concurrent.atomic.AtomicReference;

public final class ConsoleStatusAppSmokeTest {
    public static void main(String[] args) throws Exception {
        AtomicReference<Throwable> failure = new AtomicReference<>();
        SwingUtilities.invokeAndWait(() -> {
            try {
                JPanel root = ConsoleStatusApp.createContent();
                if (!(root.getLayout() instanceof BorderLayout)) throw new AssertionError("layout");
                if (!( ((BorderLayout) root.getLayout()).getLayoutComponent(BorderLayout.CENTER) instanceof ConsoleStatusPanel))
                    throw new AssertionError("status panel missing");
                JLabel footer = (JLabel) ((BorderLayout) root.getLayout()).getLayoutComponent(BorderLayout.SOUTH);
                if (!footer.getText().contains("disconnected")) throw new AssertionError("connection status");
            } catch (Throwable t) { failure.set(t); }
        });
        if (failure.get() != null) throw new AssertionError(failure.get());
    }
}
