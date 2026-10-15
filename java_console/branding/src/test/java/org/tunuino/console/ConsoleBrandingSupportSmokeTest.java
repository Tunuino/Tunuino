package org.tunuino.console;

import javax.swing.*;

public final class ConsoleBrandingSupportSmokeTest {
    public static void main(String[] args) {
        JLabel label = ConsoleBrandingSupport.createLogoLabel();
        if (!ConsoleBranding.APPLICATION_NAME.equals(label.getToolTipText())) {
            throw new AssertionError("Incorrect logo tooltip");
        }
        ImageIcon icon = ConsoleBrandingSupport.getBundleSpecificIcon();
        if (icon.getIconWidth() != 70 || icon.getIconHeight() != 70) {
            throw new AssertionError("Unexpected logo dimensions");
        }
    }
}
