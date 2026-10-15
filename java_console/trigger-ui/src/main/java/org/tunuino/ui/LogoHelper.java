package org.tunuino.ui;

import org.tunuino.console.ConsoleBranding;
import org.tunuino.console.TunuinoArtwork;

import javax.swing.*;
import java.awt.*;

/** Console logo integration point, corresponding to upstream LogoHelper. */
public final class LogoHelper {
    public static final String LINK_TEXT = ConsoleBranding.APPLICATION_NAME;

    private LogoHelper() { }

    public static JLabel createLogoLabel() {
        JLabel logo = new JLabel(getBundleSpecificIcon());
        logo.setBorder(BorderFactory.createEmptyBorder(0, 0, 0, 10));
        logo.setToolTipText(ConsoleBranding.APPLICATION_NAME);
        return logo;
    }

    public static ImageIcon getBundleSpecificIcon() {
        return TunuinoArtwork.icon(70);
    }
}
