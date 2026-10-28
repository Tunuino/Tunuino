/* Copyright (c) 2026 Tunuino. */
package io.tunuino.console;

import javafx.scene.canvas.Canvas;
import javafx.scene.layout.Region;

/** A drawing follows the allocated content area, without feeding its size back into layout. */
final class CanvasPane extends Region {
    private final Canvas canvas;

    CanvasPane(Canvas canvas) {
        this.canvas=canvas;
        setMinSize(0,0);
        setPrefSize(canvas.getWidth(),canvas.getHeight());
        canvas.setManaged(false);
        getChildren().add(canvas);
    }

    @Override protected void layoutChildren() {
        double left=snappedLeftInset(),top=snappedTopInset();
        canvas.relocate(left,top);
        canvas.setWidth(Math.max(0,getWidth()-left-snappedRightInset()));
        canvas.setHeight(Math.max(0,getHeight()-top-snappedBottomInset()));
    }
}
