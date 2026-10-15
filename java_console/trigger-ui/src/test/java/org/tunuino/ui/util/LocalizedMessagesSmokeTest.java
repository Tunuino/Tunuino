package org.tunuino.ui.util;

public final class LocalizedMessagesSmokeTest {
    public static void main(String[] args) {
        if (!"clear".equals(LocalizedMessages.CLEAR.getMessage())) throw new AssertionError("clear");
        if (!"pause".equals(LocalizedMessages.PAUSE.getMessage())) throw new AssertionError("pause");
        if (!"resume".equals(LocalizedMessages.RESUME.getMessage())) throw new AssertionError("resume");
        if (!"reset".equals(LocalizedMessages.RESET.getMessage())) throw new AssertionError("reset");
    }
}
