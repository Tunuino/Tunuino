/* Copyright (c) 2026 Tunuino. */
package io.tunuino.datalog;

public record LogSample(long timeMs, double rpm, double mapKpa, double lambda, double coolantC) {
    public LogSample {
        if (timeMs < 0 || !Double.isFinite(rpm) || rpm < 0
                || !Double.isFinite(mapKpa) || mapKpa < 0
                || !Double.isFinite(lambda) || lambda < 0 || !Double.isFinite(coolantC)) {
            throw new IllegalArgumentException("Amostra inválida");
        }
    }
}
