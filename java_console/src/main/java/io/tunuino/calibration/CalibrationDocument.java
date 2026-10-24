/* Copyright (c) 2026 Tunuino. */
package io.tunuino.calibration;

import java.io.IOException;
import java.nio.file.Path;
import java.util.ArrayDeque;
import java.util.Arrays;
import java.util.Objects;

/** Offline edit history over immutable snapshots, with a distinct saved baseline. */
public final class CalibrationDocument {
    public static final int HISTORY_LIMIT = 100;
    private final ArrayDeque<CalibrationMap> undo = new ArrayDeque<>();
    private final ArrayDeque<CalibrationMap> redo = new ArrayDeque<>();
    private CalibrationMap current;
    private CalibrationMap saved;
    private Path savedPath;

    public CalibrationDocument(CalibrationMap initial) {
        current = saved = Objects.requireNonNull(initial, "initial");
    }
    public synchronized CalibrationMap snapshot() { return current; }
    public synchronized boolean canUndo() { return !undo.isEmpty(); }
    public synchronized boolean canRedo() { return !redo.isEmpty(); }
    public synchronized Path savedPath() { return savedPath; }
    public synchronized boolean isModified() {
        return current != saved && !Arrays.equals(current.encode(), saved.encode());
    }
    public synchronized boolean editCell(int row, int column, String text) {
        CalibrationMap changed = current.withCell(row, column, text);
        if (changed == current) { return false; }
        undo.addLast(current);
        if (undo.size() > HISTORY_LIMIT) { undo.removeFirst(); }
        current = changed;
        redo.clear();
        return true;
    }
    public synchronized boolean undo() {
        if (undo.isEmpty()) { return false; }
        redo.addLast(current);
        current = undo.removeLast();
        return true;
    }
    public synchronized boolean redo() {
        if (redo.isEmpty()) { return false; }
        undo.addLast(current);
        current = redo.removeLast();
        return true;
    }
    public void saveAsNew(Path destination) throws IOException {
        CalibrationMap writing;
        synchronized (this) { writing = current; }
        writing.writeNew(destination);
        synchronized (this) {
            saved = writing;
            savedPath = destination;
        }
    }
}
