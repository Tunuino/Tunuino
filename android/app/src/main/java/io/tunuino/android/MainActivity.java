/* Copyright (c) 2026 Tunuino. */
package io.tunuino.android;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbManager;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class MainActivity extends Activity {
    private static final String USB_PERMISSION = "io.tunuino.android.USB_PERMISSION";
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private final Object connectionLock = new Object();
    private final List<UsbDevice> devices = new ArrayList<>();
    private UsbManager manager;
    private Spinner selector;
    private TextView status, identity;
    private Button connect, read, disconnect;
    private volatile UsbCdcConnection connection;
    private volatile int generation;
    private boolean active, busy;
    private String deviceName, permissionDevice;

    private final BroadcastReceiver usbEvents = new BroadcastReceiver() {
        @Override public void onReceive(Context context, Intent intent) {
            if (USB_PERMISSION.equals(intent.getAction())) {
                String requested = permissionDevice;
                permissionDevice = null;
                UsbDevice device = requested == null ? null : manager.getDeviceList().get(requested);
                if (device != null && manager.hasPermission(device)) open(device);
                else { busy = false; status.setText("Permissão USB recusada ou dispositivo desligado."); updateButtons(); }
            } else {
                if (deviceName != null && !manager.getDeviceList().containsKey(deviceName)) {
                    closeConnection();
                    status.setText("ECU desligada. Volta a ligar o cabo USB.");
                }
                refreshDevices();
            }
        }
    };

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        manager = (UsbManager) getSystemService(USB_SERVICE);
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(dp(24), dp(24), dp(24), dp(24));
        layout.setOnApplyWindowInsetsListener((view, insets) -> {
            view.setPadding(dp(24) + insets.getSystemWindowInsetLeft(), dp(24) + insets.getSystemWindowInsetTop(),
                dp(24) + insets.getSystemWindowInsetRight(), dp(24) + insets.getSystemWindowInsetBottom());
            return insets;
        });
        ImageView logo = new ImageView(this);
        logo.setImageResource(R.drawable.tunuino);
        logo.setContentDescription(getString(R.string.app_name));
        logo.setAdjustViewBounds(true);
        layout.addView(logo, new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, dp(110)));
        TextView title = text("Tunuino · USB", 25);
        layout.addView(title);
        layout.addView(text("Identificação da ECU por USB OTG · offline", 16));
        selector = new Spinner(this);
        layout.addView(selector);
        Button refresh = button("Procurar dispositivos", view -> refreshDevices());
        layout.addView(refresh);
        connect = button("Ligar à ECU", view -> requestConnection()); layout.addView(connect);
        read = button("Ler identificação", view -> readIdentity()); layout.addView(read);
        disconnect = button("Desligar", view -> { closeConnection(); status.setText("Ligação encerrada."); });
        layout.addView(disconnect);
        status = text("Liga a ECU com um cabo USB OTG e seleciona o dispositivo.", 16); layout.addView(status);
        identity = text("", 18); identity.setTextIsSelectable(true); layout.addView(identity);
        layout.addView(text("Esta versão lê identificação, revisão, firmware e UID. O firmware de bancada não "
            + "disponibiliza sensores, mapas nem afinação. A ligação Android ainda requer teste na ECU.", 14));
        String commit = BuildConfig.SOURCE_COMMIT;
        layout.addView(text("Build " + commit.substring(0, Math.min(7, commit.length())), 12));
        ScrollView scroll = new ScrollView(this); scroll.setFillViewport(true); scroll.addView(layout);
        setContentView(scroll);
    }

    @Override protected void onStart() {
        super.onStart(); active = true;
        IntentFilter filter = new IntentFilter(USB_PERMISSION);
        filter.addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED);
        filter.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED);
        if (Build.VERSION.SDK_INT >= 33) registerReceiver(usbEvents, filter, Context.RECEIVER_NOT_EXPORTED);
        else registerReceiver(usbEvents, filter);
        refreshDevices();
    }

    @Override protected void onNewIntent(Intent intent) { super.onNewIntent(intent); setIntent(intent); refreshDevices(); }

    private void refreshDevices() {
        String previous = selector.getSelectedItemPosition() >= 0 && selector.getSelectedItemPosition() < devices.size()
            ? devices.get(selector.getSelectedItemPosition()).getDeviceName() : null;
        devices.clear();
        for (UsbDevice device : manager.getDeviceList().values()) {
            if (UsbCdcConnection.isApplication(device) || UsbCdcConnection.isDfu(device)) devices.add(device);
        }
        devices.sort(Comparator.comparing(UsbDevice::getDeviceName));
        List<String> labels = new ArrayList<>();
        int selected = 0;
        for (int i = 0; i < devices.size(); i++) {
            UsbDevice device = devices.get(i);
            labels.add((UsbCdcConnection.isDfu(device) ? "STM32 DFU" : "Tunuino USB CDC") + " · " + device.getDeviceName());
            if (device.getDeviceName().equals(previous)) selected = i;
        }
        if (labels.isEmpty()) labels.add("Nenhuma ECU encontrada");
        ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item, labels);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        selector.setAdapter(adapter); selector.setSelection(selected);
        updateButtons();
    }

    private void requestConnection() {
        int index = selector.getSelectedItemPosition();
        if (!active || busy || index < 0 || index >= devices.size()) return;
        UsbDevice device = devices.get(index);
        if (UsbCdcConnection.isDfu(device)) {
            status.setText("ECU em DFU. Repõe BOOT0 na posição normal e faz reset para iniciar o firmware USB CDC.");
            return;
        }
        busy = true; updateButtons();
        if (manager.hasPermission(device)) { open(device); return; }
        permissionDevice = device.getDeviceName();
        PendingIntent permission = PendingIntent.getBroadcast(this, 0,
            new Intent(USB_PERMISSION).setPackage(getPackageName()), PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
        status.setText("A aguardar autorização USB do Android…");
        manager.requestPermission(device, permission);
    }

    private void open(UsbDevice device) {
        if (!active) return;
        int requestGeneration = ++generation;
        busy = true; deviceName = device.getDeviceName(); updateButtons();
        status.setText("A ligar e a identificar a ECU…");
        worker.execute(() -> {
            UsbCdcConnection opened = null;
            try {
                opened = new UsbCdcConnection(manager, device);
                synchronized (connectionLock) {
                    if (generation != requestGeneration) { opened.close(); return; }
                    connection = opened;
                }
                showIdentity(requestGeneration, BenchProtocol.identify(opened));
            } catch (Exception error) {
                if (opened != null) opened.close();
                showFailure(requestGeneration, error);
            }
        });
    }

    private void readIdentity() {
        UsbCdcConnection current = connection;
        if (current == null || busy) return;
        int requestGeneration = generation;
        busy = true; updateButtons();
        worker.execute(() -> {
            try { showIdentity(requestGeneration, BenchProtocol.identify(current)); }
            catch (Exception error) { current.close(); showFailure(requestGeneration, error); }
        });
    }

    private void showIdentity(int requestGeneration, BenchProtocol.Identity value) {
        runOnUiThread(() -> {
            if (!active || generation != requestGeneration) return;
            busy = false;
            status.setText("Ligação USB confirmada. Identificação recebida da ECU.");
            identity.setText(String.format(Locale.ROOT, "%s · %s\nRevisão hardware: %d\nFirmware: %s\nUID: %s\nCapacidades: 0x%02x",
                value.family, value.mcu, value.revision, value.version, value.uid, value.capabilities));
            updateButtons();
        });
    }

    private void showFailure(int requestGeneration, Exception error) {
        runOnUiThread(() -> {
            if (!active || generation != requestGeneration) return;
            closeConnection();
            status.setText("Falha USB: " + (error.getMessage() == null ? error.getClass().getSimpleName() : error.getMessage()));
        });
    }

    private void closeConnection() {
        UsbCdcConnection current;
        synchronized (connectionLock) {
            generation++;
            current = connection; connection = null;
        }
        permissionDevice = null; busy = false; deviceName = null;
        if (current != null) current.close();
        identity.setText(""); updateButtons();
    }

    private void updateButtons() {
        boolean connected = connection != null;
        selector.setEnabled(!busy && !connected);
        connect.setEnabled(active && !busy && !connected && !devices.isEmpty());
        read.setEnabled(active && !busy && connected);
        disconnect.setEnabled(connected || busy);
    }

    @Override protected void onStop() {
        active = false; unregisterReceiver(usbEvents); closeConnection();
        status.setText("Ligação encerrada ao sair da aplicação."); super.onStop();
    }

    @Override protected void onDestroy() { worker.shutdownNow(); super.onDestroy(); }

    private int dp(int value) { return Math.round(value * getResources().getDisplayMetrics().density); }
    private TextView text(String value, int size) {
        TextView view = new TextView(this); view.setText(value); view.setTextSize(size);
        view.setPadding(0, dp(10), 0, dp(10)); return view;
    }
    private Button button(String label, View.OnClickListener action) {
        Button view = new Button(this); view.setText(label); view.setOnClickListener(action); return view;
    }
}
