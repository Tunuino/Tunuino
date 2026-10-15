package org.tunuino.console;

import javax.swing.*;
import java.awt.*;

/** Reusable branding integration for Swing windows migrated from the upstream Console. */
public final class ConsoleBrandingSupport {
    private ConsoleBrandingSupport() { }

    public static void applyTo(Window window) {
        if (window == null) throw new IllegalArgumentException("window");
        if (window instanceof Frame) {
            Frame frame = (Frame) window;
            frame.setIconImage(TunuinoArtwork.icon(64).getImage());
            frame.setTitle(ConsoleBranding.APPLICATION_NAME);
        } else if (window instanceof Dialog) {
            ((Dialog) window).setTitle(ConsoleBranding.APPLICATION_NAME);
        }
    }

    public static JLabel createLogoLabel() {
        JLabel label = new JLabel(TunuinoArtwork.icon(70));
        label.setToolTipText(ConsoleBranding.APPLICATION_NAME);
        return label;
    }

    public static ImageIcon getBundleSpecificIcon() {
        // All ECU hardware variants use the Tunuino identity.
        return TunuinoArtwork.icon(70);
    }
}
