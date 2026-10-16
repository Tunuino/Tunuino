package org.tunuino.console;

public final class LambdaSampleFilterSmokeTest {
    private LambdaSampleFilterSmokeTest() { }

    private static void check(boolean actual, boolean expected) {
        if (actual != expected) throw new AssertionError("Unexpected lambda sample acceptance");
    }

    public static void main(String[] args) {
        check(LambdaSampleFilter.accept(1.02, 1.0, 2500, 80, 90, 40, 1), true);
        check(LambdaSampleFilter.accept(1.02, 1.0, 2500, 80, 60, 40, 1), false);
        check(LambdaSampleFilter.accept(1.02, 1.0, 2500, 80, 90, 400, 1), false);
        check(LambdaSampleFilter.accept(1.02, 1.0, 2500, 80, 90, 40, 8), false);
        check(LambdaSampleFilter.accept(Double.NaN, 1.0, 2500, 80, 90, 40, 1), false);
        check(LambdaSampleFilter.accept(1.5, 1.0, 2500, 80, 90, 40, 1), false);
        check(LambdaSampleFilter.accept(1.02, 0, 2500, 80, 90, 40, 1), false);
        check(LambdaSampleFilter.accept(1.02, 1.0, 0, 80, 90, 40, 1), false);
        System.out.println("LambdaSampleFilterSmokeTest passed");
    }
}
