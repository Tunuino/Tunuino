/* Copyright (c) 2026 Tunuino. */
package io.tunuino.datalog;

import java.util.Locale;
import java.util.function.ToDoubleFunction;

public enum LogChannel {
    RPM("RPM", "rpm", 0, LogSample::rpm),
    MAP("MAP", "kPa", 2, LogSample::mapKpa),
    LAMBDA("Lambda", "λ", 3, LogSample::lambda),
    COOLANT("Líquido de refrigeração", "°C", 1, LogSample::coolantC);

    private final String label;
    private final String unit;
    private final int decimals;
    private final ToDoubleFunction<LogSample> value;
    LogChannel(String label, String unit, int decimals, ToDoubleFunction<LogSample> value) {
        this.label = label; this.unit = unit; this.decimals = decimals; this.value = value;
    }
    public String unit() { return unit; }
    public double value(LogSample sample) { return value.applyAsDouble(sample); }
    public String format(LogSample sample) {
        return String.format(Locale.ROOT, "%." + decimals + "f", value(sample));
    }
    @Override public String toString() { return label; }
}
