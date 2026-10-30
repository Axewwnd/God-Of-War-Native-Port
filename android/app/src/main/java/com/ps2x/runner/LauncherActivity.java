package com.ps2x.runner;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Build;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import org.json.JSONArray;
import org.json.JSONObject;
import java.io.File;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

/** Makes the experimental driver test and its report usable without ADB. */
public final class LauncherActivity extends Activity {
    private static final int SAVE_REPORT = 1;
    private TextView status;
    private boolean testStarting;

    private File reportFile() {
        File external = getExternalFilesDir(null);
        File file = new File(external != null ? external : getFilesDir(), "ares-vulkan-report.json");
        return file.exists() ? file : new File(getFilesDir(), "ares-vulkan-report.json");
    }

    @Override public void onCreate(Bundle savedState) {
        super.onCreate(savedState);
        LinearLayout content = new LinearLayout(this);
        content.setOrientation(LinearLayout.VERTICAL);
        int padding = (int)(24 * getResources().getDisplayMetrics().density);
        content.setPadding(padding, padding, padding, padding);
        TextView title = new TextView(this);
        title.setText("PROJECT ARES · experimental");
        title.setTextSize(24);
        content.addView(title);
        TextView explanation = new TextView(this);
        explanation.setText("Esta versión permite probar Turnip en tu teléfono. El renderizador del juego para Android sigue pendiente; todavía no es una versión jugable.");
        content.addView(explanation);
        status = new TextView(this);
        status.setPadding(0, padding, 0, padding);
        content.addView(status);
        Button turnip = new Button(this);
        turnip.setText("Probar Turnip incluido");
        turnip.setOnClickListener(view -> startDriverTest("turnip"));
        content.addView(turnip);
        Button system = new Button(this);
        system.setText("Probar controlador del sistema");
        system.setOnClickListener(view -> startDriverTest("system"));
        content.addView(system);
        Button save = new Button(this);
        save.setText("Guardar informe de la última prueba");
        save.setOnClickListener(view -> {
            if (!reportFile().isFile()) { showMessage("Primero ejecuta una prueba del controlador."); return; }
            Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("application/json");
            intent.putExtra(Intent.EXTRA_TITLE, "ares-vulkan-report.json");
            startActivityForResult(intent, SAVE_REPORT);
        });
        content.addView(save);
        ScrollView scroll = new ScrollView(this);
        scroll.addView(content);
        setContentView(scroll);
    }

    private void startDriverTest(String driver) {
        if (testStarting) return;
        testStarting = true;
        try {
            File external = getExternalFilesDir(null);
            File directory = external != null ? external : getFilesDir();
            Files.write(new File(directory, "ares-driver.txt").toPath(), driver.getBytes(StandardCharsets.UTF_8));
            // Remove a previous report: a native crash must never look like an older success.
            Files.deleteIfExists(new File(directory, "ares-vulkan-report.json").toPath());
            Files.deleteIfExists(new File(getFilesDir(), "ares-vulkan-report.json").toPath());
            Files.write(new File(getFilesDir(), "ares-diagnostic-only").toPath(), new byte[]{1});
            Intent intent = new Intent();
            intent.setClassName(this, "android.app.NativeActivity");
            intent.putExtra("ares_driver_diagnostic", true);
            startActivity(intent);
        } catch (Exception error) {
            testStarting = false;
            showMessage("No se pudo iniciar la prueba: " + error.getMessage());
        }
    }

    @Override protected void onResume() {
        super.onResume();
        testStarting = false;
        if (status == null) return;
        File file = reportFile();
        if (!file.isFile()) { status.setText("Sin informe. Ejecuta una prueba para consultar el controlador y sus capacidades."); return; }
        try {
            JSONObject report = new JSONObject(new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8));
            String selected = report.optString("selected", "unavailable");
            if (report.optString("status").equals("in-progress")) {
                status.setText("La última prueba se interrumpió antes de terminar. Guarda el informe para investigar el cierre.");
                return;
            }
            StringBuilder text = new StringBuilder("Última prueba: ");
            text.append(selected.equals("turnip") ? "Turnip confirmado" : selected.equals("system") ? "controlador del sistema" : "sin controlador utilizable");
            if (report.optString("requested").equals("turnip") && !selected.equals("turnip"))
                text.append("\nTurnip no se activó. El informe conserva el error.");
            JSONArray attempts = report.optJSONArray("attempts");
            if (attempts != null) for (int i = 0; i < attempts.length(); ++i) {
                JSONObject attempt = attempts.getJSONObject(i);
                if (!attempt.optBoolean("accepted")) continue;
                JSONObject probe = attempt.optJSONObject("probe");
                JSONArray devices = probe != null ? probe.optJSONArray("devices") : null;
                if (devices != null) for (int d = 0; d < devices.length(); ++d) {
                    JSONObject device = devices.getJSONObject(d);
                    text.append("\n").append(device.optString("name")).append(" · Vulkan ").append(device.optString("api"));
                }
            }
            text.append("\nLa prueba no verifica gráficos del juego ni rendimiento.");
            status.setText(text.toString());
        } catch (Exception error) { status.setText("No se pudo leer el informe: " + error.getMessage()); }
    }

    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != SAVE_REPORT || result != RESULT_OK || data == null) return;
        Uri uri = data.getData();
        if (uri == null) return;
        try (OutputStream output = getContentResolver().openOutputStream(uri)) {
            if (output == null) throw new java.io.IOException("El destino no está disponible");
            JSONObject report = new JSONObject(new String(Files.readAllBytes(reportFile().toPath()), StandardCharsets.UTF_8));
            JSONObject android = new JSONObject();
            android.put("manufacturer", Build.MANUFACTURER);
            android.put("model", Build.MODEL);
            android.put("device", Build.DEVICE);
            android.put("hardware", Build.HARDWARE);
            android.put("release", Build.VERSION.RELEASE);
            android.put("sdk_int", Build.VERSION.SDK_INT);
            report.put("android", android);
            File log = new File(reportFile().getParentFile(), "ares-vulkan-report.log");
            if (log.isFile()) report.put("startup_log", new String(Files.readAllBytes(log.toPath()), StandardCharsets.UTF_8));
            output.write(report.toString(2).getBytes(StandardCharsets.UTF_8));
            showMessage("Informe guardado.");
        } catch (Exception error) { showMessage("No se pudo guardar el informe: " + error.getMessage()); }
    }

    private void showMessage(String message) {
        new AlertDialog.Builder(this).setMessage(message).setPositiveButton("Aceptar", null).show();
    }
}
