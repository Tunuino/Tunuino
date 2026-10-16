package org.tunuino.console;

/**
 * Stateless acceptance gate for candidate Autotune lambda samples.
 * Conservative defaults; thresholds require validation on actual hardware.
 */
public final class LambdaSampleFilter {
    private LambdaSampleFilter() { }

    /**
     * Only accept stable, warm, closed-loop-independent operating points.
     * No ECU communication or calibration writes occur here.
     */
    public static boolean accept(double measuredLambda, double targetLambda,
                                 double rpm, double mapKpa,
                                 double coolantC, double rpmChangePerSecond,
                                 double throttleChangePerSecond) {
        return Double.isFinite(measuredLambda) && measuredLambda >= 0.70
                && measuredLambda <= 1.30
                && Double.isFinite(targetLambda) && targetLambda >= 0.70
                && targetLambda <= 1.30
                && Double.isFinite(rpm) && rpm >= 800 && rpm <= 9000
                && Double.isFinite(mapKpa) && mapKpa >= 20 && mapKpa <= 250
                && Double.isFinite(coolantC) && coolantC >= 70 && coolantC <= 115
                && Double.isFinite(rpmChangePerSecond)
                && Math.abs(rpmChangePerSecond) <= 150
                && Double.isFinite(throttleChangePerSecond)
                && Math.abs(throttleChangePerSecond) <= 3;
    }
}
