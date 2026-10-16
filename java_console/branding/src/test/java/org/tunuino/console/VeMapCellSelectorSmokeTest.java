package org.tunuino.console;

public final class VeMapCellSelectorSmokeTest {
    private VeMapCellSelectorSmokeTest() { }

    private static void check(boolean condition) {
        if (!condition) throw new AssertionError("Unexpected cell selection");
    }

    private static void invalid(double[] bins, double value) {
        try {
            VeMapCellSelector.nearestIndex(bins, value);
            throw new AssertionError("Expected invalid bins or value");
        } catch (IllegalArgumentException expected) {
            // Rejected.
        }
    }

    public static void main(String[] args) {
        double[] rpm = {800, 1500, 2500, 3500};
        double[] map = {20, 50, 80, 110};
        VeMapCellSelector.Cell cell = VeMapCellSelector.nearest(rpm, map, 2400, 78);
        check(cell.rpmIndex == 2 && cell.mapIndex == 2);
        check(VeMapCellSelector.nearestIndex(rpm, 100) == 0);
        check(VeMapCellSelector.nearestIndex(rpm, 9999) == 3);
        check(VeMapCellSelector.nearestIndex(rpm, 2000) == 1);
        invalid(null, 10);
        invalid(new double[0], 10);
        invalid(new double[] {1, 1}, 1);
        invalid(new double[] {2, 1}, 1);
        invalid(new double[] {1, Double.NaN}, 1);
        invalid(new double[] {1, 2}, Double.POSITIVE_INFINITY);
        System.out.println("VeMapCellSelectorSmokeTest passed");
    }
}
