package org.tunuino.console;

/** Selects the nearest VE table cell without changing any calibration data. */
public final class VeMapCellSelector {
    private VeMapCellSelector() { }

    public static final class Cell {
        public final int rpmIndex;
        public final int mapIndex;

        private Cell(int rpmIndex, int mapIndex) {
            this.rpmIndex = rpmIndex;
            this.mapIndex = mapIndex;
        }
    }

    public static Cell nearest(double[] rpmBins, double[] mapBins,
                               double rpm, double mapKpa) {
        return new Cell(nearestIndex(rpmBins, rpm), nearestIndex(mapBins, mapKpa));
    }

    public static int nearestIndex(double[] bins, double value) {
        if (bins == null || bins.length == 0 || !Double.isFinite(value)) {
            throw new IllegalArgumentException("Nonempty bins and finite value required");
        }
        int nearest = 0;
        double distance = Double.POSITIVE_INFINITY;
        for (int i = 0; i < bins.length; i++) {
            if (!Double.isFinite(bins[i]) || (i > 0 && bins[i] <= bins[i - 1])) {
                throw new IllegalArgumentException("Bins must be finite and strictly ascending");
            }
            double delta = Math.abs(value - bins[i]);
            if (delta < distance) {
                distance = delta;
                nearest = i;
            }
        }
        return nearest;
    }
}
