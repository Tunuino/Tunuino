/* Copyright (c) 2026 Tunuino. */
package io.tunuino.transport;

import java.io.Closeable;
import java.io.IOException;

/** A single serial endpoint. Zero indicates timeout; negative indicates loss of connection. */
public interface SerialChannel extends Closeable {
    int read(byte[] buffer, int offset, int count, int timeoutMs) throws IOException;
    int write(byte[] buffer, int offset, int count, int timeoutMs) throws IOException;
}
