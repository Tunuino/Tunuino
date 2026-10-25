/* Copyright (c) 2026 Tunuino. */
package io.tunuino.datalog;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/** Index-based inspection preserves samples with equal timestamps. */
public final class DatalogAnalysis {
    public record Range(double minimum, double maximum) {}
    private DatalogAnalysis() {}

    public static Range range(List<LogSample> samples, LogChannel channel) {
        if (samples.isEmpty()) throw new IllegalArgumentException("O datalog não contém amostras");
        double minimum = Double.POSITIVE_INFINITY, maximum = Double.NEGATIVE_INFINITY;
        for (LogSample sample : samples) {
            if (!channel.available(sample)) continue;
            double value = channel.value(sample);
            minimum = Math.min(minimum, value); maximum = Math.max(maximum, value);
        }
        if (minimum == Double.POSITIVE_INFINITY) return new Range(Double.NaN, Double.NaN);
        return new Range(minimum, maximum);
    }

    /** Ordered bucket extrema keep short peaks that a fixed stride would hide. */
    public static List<LogSample> chartSamples(List<LogSample> samples, LogChannel channel, int limit) {
        Objects.requireNonNull(channel, "channel");
        if (limit < 4) throw new IllegalArgumentException("O gráfico exige pelo menos quatro pontos");
        if (samples.size() <= limit) return List.copyOf(samples);
        List<LogSample> reduced = new ArrayList<>(limit);
        reduced.add(samples.get(0));
        int buckets = (limit - 2) / 2;
        int interior = samples.size() - 2;
        for (int bucket = 0; bucket < buckets; ++bucket) {
            int from = 1 + (int) ((long) bucket * interior / buckets);
            int to = 1 + (int) ((long) (bucket + 1) * interior / buckets);
            int low = from, high = from;
            for (int i = from + 1; i < to; ++i) {
                if (channel.value(samples.get(i)) < channel.value(samples.get(low))) low = i;
                if (channel.value(samples.get(i)) > channel.value(samples.get(high))) high = i;
            }
            reduced.add(samples.get(Math.min(low, high)));
            if (low != high) reduced.add(samples.get(Math.max(low, high)));
        }
        reduced.add(samples.get(samples.size() - 1));
        return List.copyOf(reduced);
    }
}
