package org.tunuino.ui;

import javax.swing.*;

public final class LogoHelperSmokeTest {
    public static void main(String[] args) {
        JLabel label = LogoHelper.createLogoLabel();
        if (label.getIcon() == null || label.getIcon().getIconWidth() != 70) {
            throw new AssertionError("Missing Tunuino logo");
        }
        if (!"Tunuino Console".equals(LogoHelper.LINK_TEXT)) {
            throw new AssertionError("Incorrect branding");
        }
    }
}
