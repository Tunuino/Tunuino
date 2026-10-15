package org.tunuino.console;

/** Standalone assertions for the offline Autotune VE estimator. */
public final class AutotuneCalculatorSmokeTest {
    private AutotuneCalculatorSmokeTest() { }

    private static void expect(double actual, double expected) {
        if (Math.abs(actual - expected) > 1e-9) {
            throw new AssertionError("Expected " + expected + ", got " + actual);
        }
    }

    private static void invalid(double ve, double measured, double target, double limit) {
        try {
            AutotuneCalculator.estimateVe(ve, measured, target, limit);
            throw new AssertionError("Expected IllegalArgumentException");
        } catch (IllegalArgumentException expected) {
            // Invalid inputs must be rejected.
        }
    }

    public static void main(String[] args) {
        expect(AutotuneCalculator.estimateVe(50, 1.05, 1.0, 0.10), 52.5);
        expect(AutotuneCalculator.estimateVe(50, 0.95, 1.0, 0.10), 47.5);
        expect(AutotuneCalculator.estimateVe(50, 1.50, 1.0, 0.10), 55.0);
        expect(AutotuneCalculator.estimateVe(50, 0.50, 1.0, 0.10), 45.0);
        expect(AutotuneCalculator.estimateVe(50, 1.10, 1.0, 0.0), 50.0);
        invalid(0, 1, 1, 0.1);
        invalid(50, Double.NaN, 1, 0.1);
        invalid(50, 1, 0, 0.1);
        invalid(50, 1, 1, 0.26);
        invalid(50, 1, 1, -0.01);
        invalid(Double.POSITIVE_INFINITY, 1, 1, 0.1);
        System.out.println("AutotuneCalculatorSmokeTest passed");
    }
}
