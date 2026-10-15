package org.tunuino.console;

/**
 * Offline VE correction estimator. This class never communicates with an ECU
 * or writes calibration values. Inputs must come from validated measurements.
 */
public final class AutotuneCalculator {
    private AutotuneCalculator() { }

    /**
     * Estimate a VE correction from measured and target lambda.
     * The relative change is clamped to maxChangeFraction (e.g. 0.10 = 10%).
     */
    public static double estimateVe(double currentVe, double measuredLambda,
                                    double targetLambda, double maxChangeFraction) {
        requirePositiveFinite("currentVe", currentVe);
        requirePositiveFinite("measuredLambda", measuredLambda);
        requirePositiveFinite("targetLambda", targetLambda);
        if (!Double.isFinite(maxChangeFraction)
                || maxChangeFraction < 0.0 || maxChangeFraction > 0.25) {
            throw new IllegalArgumentException("maxChangeFraction must be between 0 and 0.25");
        }
        double ratio = measuredLambda / targetLambda;
        double boundedRatio = Math.max(1.0 - maxChangeFraction,
                Math.min(1.0 + maxChangeFraction, ratio));
        double result = currentVe * boundedRatio;
        if (!Double.isFinite(result)) {
            throw new IllegalArgumentException("VE correction overflow");
        }
        return result;
    }

    private static void requirePositiveFinite(String name, double value) {
        if (!Double.isFinite(value) || value <= 0.0) {
            throw new IllegalArgumentException(name + " must be positive and finite");
        }
    }
}
