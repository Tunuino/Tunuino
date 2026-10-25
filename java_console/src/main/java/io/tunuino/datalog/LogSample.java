/* Copyright (c) 2026 Tunuino. */
package io.tunuino.datalog;

public record LogSample(long timeMs, double rpm, double mapKpa, double lambda, double coolantC, int validMask) {
    public LogSample(long timeMs, double rpm, double mapKpa, double lambda, double coolantC) {
        this(timeMs,rpm,mapKpa,lambda,coolantC,15);
    }
    public LogSample {
        if (timeMs < 0 || !Double.isFinite(rpm) || rpm < 0
                || !Double.isFinite(mapKpa) || mapKpa < 0
                || !Double.isFinite(lambda) || lambda < 0 || !Double.isFinite(coolantC)
                || validMask<0 || validMask>15
                || ((validMask&1)==0 && rpm!=0) || ((validMask&2)==0 && mapKpa!=0)
                || ((validMask&4)==0 && lambda!=0) || ((validMask&8)==0 && coolantC!=0)) {
            throw new IllegalArgumentException("Amostra inválida");
        }
    }
}
