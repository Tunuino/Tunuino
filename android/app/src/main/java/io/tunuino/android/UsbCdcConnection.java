/* Copyright (c) 2026 Tunuino. */
package io.tunuino.android;

import android.hardware.usb.UsbConstants;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbEndpoint;
import android.hardware.usb.UsbInterface;
import android.hardware.usb.UsbManager;
import java.io.Closeable;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.concurrent.atomic.AtomicBoolean;

/** Native USB Host CDC transport; retains packet tails for fragmented protocol reads. */
public final class UsbCdcConnection implements BenchProtocol.Transport, Closeable {
    private final UsbDeviceConnection connection;
    private final UsbInterface control, data;
    private final UsbEndpoint input, output;
    private final AtomicBoolean closed = new AtomicBoolean();
    private final byte[] received;
    private int position, available;

    public static boolean isApplication(UsbDevice device) {
        return device.getVendorId() == 0xcafe && device.getProductId() == 0x4001;
    }

    public static boolean isDfu(UsbDevice device) {
        return device.getVendorId() == 0x0483 && device.getProductId() == 0xdf11;
    }

    public UsbCdcConnection(UsbManager manager, UsbDevice device) throws IOException {
        if (!isApplication(device)) throw new IOException("Dispositivo USB não suportado");
        UsbInterface foundControl = null, foundData = null;
        UsbEndpoint foundInput = null, foundOutput = null;
        for (int i = 0; i < device.getInterfaceCount(); i++) {
            UsbInterface candidate = device.getInterface(i);
            if (candidate.getInterfaceClass() == UsbConstants.USB_CLASS_COMM
                    && candidate.getInterfaceSubclass() == 2) foundControl = candidate;
            if (candidate.getInterfaceClass() == UsbConstants.USB_CLASS_CDC_DATA) {
                UsbEndpoint in = null, out = null;
                for (int j = 0; j < candidate.getEndpointCount(); j++) {
                    UsbEndpoint endpoint = candidate.getEndpoint(j);
                    if (endpoint.getType() != UsbConstants.USB_ENDPOINT_XFER_BULK) continue;
                    if (endpoint.getDirection() == UsbConstants.USB_DIR_IN) in = endpoint;
                    else out = endpoint;
                }
                if (in != null && out != null) { foundData = candidate; foundInput = in; foundOutput = out; }
            }
        }
        if (foundControl == null || foundData == null) throw new IOException("Interfaces CDC não encontradas");
        control = foundControl; data = foundData; input = foundInput; output = foundOutput;
        received = new byte[Math.max(64, input.getMaxPacketSize())];
        connection = manager.openDevice(device);
        if (connection == null) throw new IOException("Acesso USB indisponível");
        boolean controlClaimed = false, dataClaimed = false;
        try {
            controlClaimed = connection.claimInterface(control, true);
            dataClaimed = connection.claimInterface(data, true);
            if (!controlClaimed || !dataClaimed) throw new IOException("Não foi possível reservar as interfaces CDC");
            byte[] coding = ByteBuffer.allocate(7).order(ByteOrder.LITTLE_ENDIAN)
                .putInt(115200).put((byte) 0).put((byte) 0).put((byte) 8).array();
            if (connection.controlTransfer(0x21, 0x20, 0, control.getId(), coding, 7, 1000) != 7
                    || connection.controlTransfer(0x21, 0x22, 3, control.getId(), null, 0, 1000) < 0) {
                throw new IOException("Configuração CDC 115200 8N1 recusada");
            }
        } catch (IOException | RuntimeException failure) {
            if (dataClaimed) connection.releaseInterface(data);
            if (controlClaimed) connection.releaseInterface(control);
            connection.close();
            throw failure;
        }
    }

    @Override public int read(byte[] bytes, int offset, int count, int timeoutMs) throws IOException {
        requireOpen();
        if (position == available) {
            available = connection.bulkTransfer(input, received, received.length, timeoutMs);
            position = 0;
            if (available <= 0) throw new IOException("Tempo de leitura USB excedido ou cabo desligado");
        }
        int size = Math.min(count, available - position);
        System.arraycopy(received, position, bytes, offset, size);
        position += size;
        return size;
    }

    @Override public int write(byte[] bytes, int offset, int count, int timeoutMs) throws IOException {
        requireOpen();
        return connection.bulkTransfer(output, bytes, offset, count, timeoutMs);
    }

    private void requireOpen() throws IOException {
        if (closed.get()) throw new IOException("Ligação USB encerrada");
    }

    @Override public void close() {
        if (closed.compareAndSet(false, true)) connection.close();
    }
}
