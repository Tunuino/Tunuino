package org.tunuino;

import com.devexperts.logging.Logging;
import java.util.logging.Level;

/** Verifies that the imported logging implementation works at runtime. */
public final class LoggingSmokeTest {
    public static void main(String[] args) {
        Logging logger = Logging.getLogging(LoggingSmokeTest.class);
        if (logger == null || logger.getName() == null || logger.getName().isEmpty()) {
            throw new AssertionError("Logger initialization failed");
        }
        logger.info("Tunuino logging API smoke test");
        logger.warn("Tunuino warning path smoke test");
    }
}
