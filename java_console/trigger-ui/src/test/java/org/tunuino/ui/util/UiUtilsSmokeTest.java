package org.tunuino.ui.util;

import javax.swing.*;
import java.awt.*;
import java.util.concurrent.atomic.AtomicReference;

public final class UiUtilsSmokeTest {
    public static void main(String[] args) throws Exception {
        AtomicReference<Throwable> failure = new AtomicReference<>();
        SwingUtilities.invokeAndWait(() -> {
            try {
                JButton pause = UiUtils.createPauseButton();
                if (!"pause".equals(pause.getText())) throw new AssertionError("pause");
                UiUtils.setPauseButtonText(pause, true);
                if (!"resume".equals(pause.getText())) throw new AssertionError("resume");
                if (!"clear".equals(UiUtils.createClearButton().getText())) throw new AssertionError("clear");
                JPanel panel = new JPanel();
                panel.add(new JLabel("nested"));
                if (UiUtils.getAllComponents(panel).size() != 1) throw new AssertionError("tree traversal");
                JComponent wrapped = UiUtils.wrap(pause);
                if (!(wrapped.getLayout() instanceof BorderLayout)) throw new AssertionError("wrapper");
                JLabel tooltip = new JLabel();
                UiUtils.setToolTip(tooltip, "one", "two");
                if (!tooltip.getToolTipText().contains("one<br>two")) throw new AssertionError("tooltip");
            } catch (Throwable t) { failure.set(t); }
        });
        if (failure.get() != null) throw new AssertionError("UI smoke test failed", failure.get());
    }
}
