package org.tunuino.console;

public final class ConsoleBrandingSmokeTest {
    public static void main(String[] args) {
        if (!"Tunuino Console".equals(ConsoleBranding.APPLICATION_NAME)) {
            throw new AssertionError("Incorrect application title");
        }
        if (!ConsoleBranding.LOGO_RESOURCE.startsWith("/org/tunuino/")) {
            throw new AssertionError("Logo must use Tunuino resource namespace");
        }
    }
}
