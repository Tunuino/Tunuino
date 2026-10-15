package org.tunuino.ui.util;

/** Console action labels, adapted from upstream trigger-ui LocalizedMessages. */
public enum LocalizedMessages {
    CLEAR("clear"),
    PAUSE("pause"),
    RESUME("resume"),
    RESET("reset"),
    ;
    private final String message;

    LocalizedMessages(String message) {
        this.message = message;
    }

    public String getMessage() {
        return message;
    }
}
