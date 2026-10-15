package org.tunuino.console;

import java.awt.image.BufferedImage;

public final class ArtworkSmokeTest {
    public static void main(String[] args) {
        if (!"Tunuino Console".equals(ConsoleBranding.APPLICATION_NAME)) throw new AssertionError("Title mismatch");
        for (int size : new int[] {16, 32, 64, 128}) {
            BufferedImage image = (BufferedImage) TunuinoArtwork.icon(size).getImage();
            if (image.getWidth() != size || image.getHeight() != size) throw new AssertionError("Invalid icon size");
            if ((image.getRGB(size / 2, size / 4) >>> 24) == 0) throw new AssertionError("Empty logo");
        }
    }
}
