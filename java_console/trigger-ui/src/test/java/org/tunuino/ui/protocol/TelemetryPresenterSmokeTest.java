package org.tunuino.ui.protocol;

import org.tunuino.ui.ConsoleStatusPanel;
import javax.swing.SwingUtilities;
import java.util.concurrent.atomic.AtomicReference;

public final class TelemetryPresenterSmokeTest {
    private static byte[] fromHex(String hex) {
        byte[] bytes = new byte[hex.length() / 2];
        for (int i = 0; i < bytes.length; i++) bytes[i] = (byte) Integer.parseInt(hex.substring(i * 2, i * 2 + 2), 16);
        return bytes;
    }

    public static void main(String[] args) throws Exception {
        AtomicReference<ConsoleStatusPanel> panelRef = new AtomicReference<>();
        SwingUtilities.invokeAndWait(() -> panelRef.set(new ConsoleStatusPanel()));
        ConsoleStatusPanel panel = panelRef.get();
        TelemetryPresenter presenter = new TelemetryPresenter(panel);
        presenter.accept(fromHex("544e01020e00dc05f5034501e2048a02be0564059dad"));
        SwingUtilities.invokeAndWait(() -> {
            if (!"1500.0 rpm".equals(panel.getReadingText("RPM"))) throw new AssertionError("rpm");
            if (!"101.3 kPa".equals(panel.getReadingText("MAP"))) throw new AssertionError("map");
            if (!"85.0 °C".equals(panel.getReadingText("Coolant"))) throw new AssertionError("coolant");
            if (!"13.8 V".equals(panel.getReadingText("Battery"))) throw new AssertionError("battery");
        });
        byte[] corrupted = fromHex("544e01020e00dc05f5034501e2048a02be0564059dad");
        corrupted[7] ^= 1;
        try {
            presenter.accept(corrupted);
            throw new AssertionError("Corrupted packet accepted");
        } catch (IllegalArgumentException expected) { }
    }
}
