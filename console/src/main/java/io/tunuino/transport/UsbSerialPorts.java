/* Copyright (c) 2026 Tunuino. */
package io.tunuino.transport;

import com.fazecast.jSerialComm.SerialPort;
import java.io.IOException;
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;

/** USB serial discovery and transport; an open port does not establish ECU identity. */
public final class UsbSerialPorts {
    private UsbSerialPorts() {}

    public record Port(String path, String name, String description, int vendorId, int productId) {
        @Override public String toString() {
            return name + " · " + description;
        }
    }

    public static List<Port> list() {
        return Arrays.stream(SerialPort.getCommPorts())
            .map(p -> new Port(p.getSystemPortPath(), p.getSystemPortName(),
                p.getDescriptivePortName(), p.getVendorID(), p.getProductID()))
            .sorted(Comparator.comparing(Port::name))
            .toList();
    }

    public static PacketSession open(Port selected) throws IOException {
        SerialPort port = SerialPort.getCommPort(selected.path());
        // Host serial settings; the USB CDC device determines actual USB throughput.
        if (!port.setComPortParameters(115200, 8, SerialPort.ONE_STOP_BIT, SerialPort.NO_PARITY)
                || !port.setFlowControl(SerialPort.FLOW_CONTROL_DISABLED)) {
            throw new IOException("Não foi possível configurar " + selected.name());
        }
        if (!port.openPort()) {
            throw new IOException("Não foi possível abrir " + selected.name());
        }
        return new PacketSession(new SerialChannel() {
            private void configure(int timeoutMs) throws IOException {
                if (!port.setComPortTimeouts(SerialPort.TIMEOUT_READ_SEMI_BLOCKING
                        | SerialPort.TIMEOUT_WRITE_BLOCKING, timeoutMs, timeoutMs)) {
                    throw new IOException("Não foi possível configurar o timeout USB");
                }
            }

            @Override public int read(byte[] bytes, int offset, int count, int timeoutMs) throws IOException {
                configure(timeoutMs);
                return port.readBytes(bytes, count, offset);
            }

            @Override public int write(byte[] bytes, int offset, int count, int timeoutMs) throws IOException {
                configure(timeoutMs);
                return port.writeBytes(bytes, count, offset);
            }

            @Override public void close() throws IOException {
                if (!port.closePort()) {
                    throw new IOException("Não foi possível encerrar " + selected.name());
                }
            }
        });
    }
}
