package org.tunuino.ui;

import org.tunuino.console.TunuinoArtwork;

import javax.swing.*;
import java.awt.*;

/** Console frame icon integration point, corresponding to upstream BasicLogoHelper. */
public final class BasicLogoHelper {
    private BasicLogoHelper() { }

    public static void setGenericFrameIcon(Frame frame) {
        setFrameIcon(frame, TunuinoArtwork.icon(64));
    }

    public static void setFrameIcon(Frame frame, ImageIcon icon) {
        if (frame != null && icon != null) frame.setIconImage(icon.getImage());
    }
}
