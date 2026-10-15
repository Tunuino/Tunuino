package org.tunuino.ui.protocol;

import java.io.ByteArrayInputStream;
import java.util.ArrayList;
import java.util.List;

public final class TelemetrySessionSmokeTest {
    public static void main(String[] args) throws Exception {
        List<TelemetrySession.State> states = new ArrayList<>();
        TelemetrySession session = new TelemetrySession(new ByteArrayInputStream(new byte[0]),
            frame -> { }, states::add, error -> { throw new AssertionError(error); });
        session.start();
        session.close();
        session.close();
        if (states.size() != 2 || states.get(0) != TelemetrySession.State.CONNECTED ||
            states.get(1) != TelemetrySession.State.DISCONNECTED)
            throw new AssertionError("Incorrect connection states: " + states);
        try {
            session.start();
            throw new AssertionError("Closed session restarted");
        } catch (IllegalStateException expected) { }
    }
}
