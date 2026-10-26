/* Copyright (c) 2026 Tunuino. */
package io.tunuino.calibration;

import java.util.List;

/** Structural templates only: calibration values belong to the actual engine and loads. */
public record EnginePreset(String name, List<Integer> firingOrder) {
    public EnginePreset {
        firingOrder = List.copyOf(firingOrder);
        if (firingOrder.isEmpty() || firingOrder.size() > 12
            || firingOrder.stream().distinct().count() != firingOrder.size()) throw new IllegalArgumentException("Ordem inválida");
        for (int i = 1; i <= firingOrder.size(); i++) if (!firingOrder.contains(i)) throw new IllegalArgumentException("Cilindro em falta");
    }
    public int cylinders() { return firingOrder.size(); }
    public int tdcCentidegrees(int cylinder) {
        int position = firingOrder.indexOf(cylinder);
        if (position < 0) throw new IllegalArgumentException("Cilindro inexistente");
        return (int) Math.round(position * 72000.0 / cylinders());
    }
    @Override public String toString() { return name; }
    public static List<EnginePreset> choices() {
        return List.of(new EnginePreset("1 cilindro · 4 tempos", List.of(1)),
            new EnginePreset("2 cilindros · 1–2 · intervalos iguais", List.of(1,2)),
            new EnginePreset("3 cilindros · 1–2–3", List.of(1,2,3)),
            new EnginePreset("4 cilindros · 1–3–4–2", List.of(1,3,4,2)),
            new EnginePreset("4 cilindros · 1–2–4–3", List.of(1,2,4,3)),
            new EnginePreset("5 cilindros · 1–2–4–5–3", List.of(1,2,4,5,3)),
            new EnginePreset("6 cilindros · 1–5–3–6–2–4", List.of(1,5,3,6,2,4)),
            new EnginePreset("8 cilindros · 1–8–4–3–6–5–7–2", List.of(1,8,4,3,6,5,7,2)));
    }
}
