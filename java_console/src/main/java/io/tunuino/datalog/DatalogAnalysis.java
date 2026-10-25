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
            if(!channel.available(sample))continue;
            double value = channel.value(sample);
            minimum = Math.min(minimum, value); maximum = Math.max(maximum, value);
        }
        return minimum==Double.POSITIVE_INFINITY?new Range(Double.NaN,Double.NaN):new Range(minimum, maximum);
    }

    /** Ordered bucket extrema keep short peaks that a fixed stride would hide. */
    public static List<LogSample> chartSamples(List<LogSample> samples, LogChannel channel, int limit) {
        Objects.requireNonNull(channel, "channel");
        if (limit < 4) throw new IllegalArgumentException("O gráfico exige pelo menos quatro pontos");
        if (samples.size() <= limit) return List.copyOf(samples);
        List<LogSample> reduced = new ArrayList<>(limit);
        reduced.add(samples.get(0));
        boolean gaps=samples.stream().anyMatch(sample -> !channel.available(sample));
        int buckets = Math.max(1,(limit - 2) / (gaps?3:2));
        int interior = samples.size() - 2;
        for (int bucket = 0; bucket < buckets; ++bucket) {
            int from = 1 + (int) ((long) bucket * interior / buckets);
            int to = 1 + (int) ((long) (bucket + 1) * interior / buckets);
            int low = -1, high = -1, missing = -1;
            for (int i = from; i < to; ++i) {
                if(!channel.available(samples.get(i))){if(missing<0)missing=i;continue;}
                if (low<0 || channel.value(samples.get(i)) < channel.value(samples.get(low))) low = i;
                if (high<0 || channel.value(samples.get(i)) > channel.value(samples.get(high))) high = i;
            }
            var positions=new java.util.TreeSet<Integer>();
            if(low>=0)positions.add(low);if(high>=0)positions.add(high);if(missing>=0)positions.add(missing);
            for(int i:positions)if(reduced.size()<limit-1)reduced.add(samples.get(i));
        }
        reduced.add(samples.get(samples.size() - 1));
        return List.copyOf(reduced);
    }
}
