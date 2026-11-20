// SPDX-License-Identifier: MIT
// FM-1 Rescue for Android: one screen that does the whole unbrick.
//   1. What to restore: JIANT (the public package, inside the APK, the default:
//      it runs under the boot head of V15 and of V16), the official V15
//      (also in the APK, hash-checked), or a .fwsc of your own. JieLi's
//      wl82loader.bin ships in the APK.
//   2. The RP2040 (RP2040-Zero, XIAO or a Pico-style board) is plugged into the phone with an OTG
//      adapter while BOOT is held: the app programs it over PICOBOOT, with the
//      V15 inside (no file copy, no computer). A board that already runs the
//      rescue firmware is sent back to BOOTSEL by the app itself.
//   3. The board reboots into the rescue firmware; the app reads its progress
//      on the data port and says what to do: switch the FM-1 on, wait, done.
// The phone only powers the RP2040; the FM-1 runs on its battery.
package fm.jiant.rescue;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.hardware.usb.UsbConstants;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbEndpoint;
import android.hardware.usb.UsbInterface;
import android.hardware.usb.UsbManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;
import java.util.TreeMap;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MainActivity extends Activity {
    private static final int PID_RESCUE = 0x000A;     // rescue / Transporter firmware
    private static final int PICK_FWSC = 1;
    private static final String ACTION_PERMISSION = "fm.jiant.rescue.USB_PERMISSION";
    private static final int ACCENT = 0xFFFF7A00;

    private final boolean es = Locale.getDefault().getLanguage().equals("es");
    private final Handler ui = new Handler(Looper.getMainLooper());
    private final ExecutorService usb = Executors.newSingleThreadExecutor();
    private final ExecutorService net = Executors.newSingleThreadExecutor();
    private UsbManager usbManager;
    private SharedPreferences prefs;

    private TextView fwscText, loaderText, stage, detail, log, diagHead, diagText;
    private volatile Diagnosis diag = new Diagnosis();
    private final StringBuilder fullLog = new StringBuilder();
    private volatile String pendingCmd;          // a data-port request from the UI (e.g. "rekey")
    private RadioButton srcJiant, srcV15, srcCustom;
    private ProgressBar progress;
    private Button reflash;

    private byte[] loader;
    private byte[] jiantPkg, v15Pkg, customPkg;     // the three sources; null until available
    private String jiantVersion = "", customLabel = "";
    private volatile boolean busy;              // a USB job is running
    private volatile String monitoring;         // device name the status link is open on
    private volatile boolean stopMonitor;
    private final Set<String> asked = new HashSet<>();
    private final Set<String> autoReset = new HashSet<>();

    private String t(String esText, String enText) { return es ? esText : enText; }

    // ------------------------------------------------------------ UI

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        usbManager = (UsbManager) getSystemService(Context.USB_SERVICE);
        prefs = getSharedPreferences("rescue", MODE_PRIVATE);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        buildUi();
        loadFiles();
        poll.run();
    }

    private int dp(int v) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, getResources().getDisplayMetrics());
    }

    private TextView text(LinearLayout parent, String s, int sp, boolean bold) {
        TextView v = new TextView(this);
        v.setText(s);
        v.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        v.setTextColor(0xFF1D1D1B);
        if (bold) v.setTypeface(Typeface.DEFAULT_BOLD);
        v.setPadding(0, dp(4), 0, dp(4));
        parent.addView(v);
        return v;
    }

    private LinearLayout card(LinearLayout parent) {
        LinearLayout c = new LinearLayout(this);
        c.setOrientation(LinearLayout.VERTICAL);
        c.setPadding(dp(16), dp(12), dp(16), dp(12));
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(Color.WHITE);
        bg.setCornerRadius(dp(12));
        bg.setStroke(dp(1), 0xFFE2E0DA);
        c.setBackground(bg);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, -2);
        lp.bottomMargin = dp(12);
        parent.addView(c, lp);
        return c;
    }

    private Button button(LinearLayout parent, String s) {
        Button v = new Button(this);
        v.setText(s);
        v.setAllCaps(false);
        parent.addView(v, new LinearLayout.LayoutParams(-2, -2));
        return v;
    }

    private void buildUi() {
        ScrollView scroll = new ScrollView(this);
        scroll.setBackgroundColor(0xFFF6F5F1);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(dp(16), dp(20), dp(16), dp(32));
        scroll.addView(root);

        TextView title = text(root, "FM-1 Rescue", 24, true);
        title.setTextColor(ACCENT);
        text(root, t("Repara un FM-1 que no enciende con un RP2040 y este móvil. Sin ordenador.",
                "Fix an FM-1 that does not turn on with an RP2040 and this phone. No computer."), 14, false);

        LinearLayout c1 = card(root);
        text(c1, t("1 · Qué firmware instalar", "1 · Which firmware to install"), 16, true);
        RadioGroup src = new RadioGroup(this);
        srcJiant = new RadioButton(this);
        srcV15 = new RadioButton(this);
        srcCustom = new RadioButton(this);
        for (RadioButton r : new RadioButton[] {srcJiant, srcV15, srcCustom}) {
            r.setId(View.generateViewId());
            src.addView(r);
        }
        srcCustom.setVisibility(View.GONE);
        switch (prefs.getString("src", "jiant")) {
            case "v15": srcV15.setChecked(true); break;
            case "custom": srcCustom.setChecked(true); break;
            default: srcJiant.setChecked(true);
        }
        src.setOnCheckedChangeListener((gr, id) -> {
            prefs.edit().putString("src", id == srcV15.getId() ? "v15" : id == srcCustom.getId() ? "custom" : "jiant").apply();
            if (id == srcV15.getId() && v15Pkg == null) net.execute(this::downloadV15);
            updateFileTexts();
        });
        c1.addView(src);
        fwscText = text(c1, "", 13, false);
        button(c1, t("Elegir otro .fwsc (V15 o JIANT)", "Choose another .fwsc (V15 or JIANT)")).setOnClickListener(v -> {
            Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            i.addCategory(Intent.CATEGORY_OPENABLE);
            i.setType("*/*");
            startActivityForResult(i, PICK_FWSC);
        });
        loaderText = text(c1, "", 12, false);

        LinearLayout c2 = card(root);
        text(c2, t("2 · Tu placa", "2 · Your board"), 16, true);
        RadioGroup g = new RadioGroup(this);
        RadioButton zero = new RadioButton(this), xiao = new RadioButton(this), pico = new RadioButton(this);
        zero.setText(t("Waveshare RP2040-Zero  (verde → 0, blanco → 1, negro → GND)",
                "Waveshare RP2040-Zero  (green → 0, white → 1, black → GND)"));
        xiao.setText(t("Seeed XIAO RP2040  (verde → D6, blanco → D7, negro → GND)",
                "Seeed XIAO RP2040  (green → D6, white → D7, black → GND)"));
        pico.setText(t("Raspberry Pi Pico, Ultimate Pico u otra placa tipo Pico  (verde → GP0 pin 1, blanco → GP1 pin 2, negro → GND pin 3)",
                "Raspberry Pi Pico, Ultimate Pico or another Pico-style board  (green → GP0 pin 1, white → GP1 pin 2, black → GND pin 3)"));
        for (RadioButton r : new RadioButton[] {zero, xiao, pico}) {
            r.setId(View.generateViewId());
            g.addView(r);
        }
        (board().equals("xiao") ? xiao : board().equals("pico") ? pico : zero).setChecked(true);
        g.setOnCheckedChangeListener((gr, id) -> prefs.edit()
                .putString("board", id == xiao.getId() ? "xiao" : id == pico.getId() ? "pico" : "zero").apply());
        c2.addView(g);

        LinearLayout c3 = card(root);
        text(c3, t("3 · Conectar", "3 · Connect"), 16, true);
        text(c3, t("• FM-1 APAGADO. Cable rojo sin conectar.\n"
                + "• Conecta los 3 cables del RP2040 al USB-C del FM-1.\n"
                + "• Enchufa el RP2040 a este móvil con un adaptador OTG (si no lo detecta: con BOOT pulsado).\n"
                + "• La app hace el resto y te dice cuándo encender el FM-1.",
                "• FM-1 OFF. Red wire unconnected.\n"
                + "• Connect the RP2040's 3 wires to the FM-1's USB-C.\n"
                + "• Plug the RP2040 into this phone with an OTG adapter (if it is not seen: holding BOOT).\n"
                + "• The app does the rest and tells you when to switch the FM-1 on."), 14, false);

        LinearLayout c4 = card(root);
        stage = text(c4, "", 20, true);
        detail = text(c4, "", 14, false);
        progress = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progress.setMax(1000);
        c4.addView(progress, new LinearLayout.LayoutParams(-1, dp(12)));
        reflash = button(c4, t("Volver a grabar el RP2040", "Reprogram the RP2040"));
        reflash.setVisibility(View.GONE);
        reflash.setOnClickListener(v -> {
            UsbDevice d = find(PID_RESCUE);
            if (d != null && usbManager.hasPermission(d)) resetToBootsel(d);
        });

        LinearLayout c5 = card(root);
        text(c5, t("Diagnóstico", "Diagnostics"), 16, true);
        diagHead = text(c5, t("Esperando el registro del RP2040", "Waiting for the RP2040's log"), 15, true);
        diagText = text(c5, t("Aparece cuando el RP2040 ya tiene el firmware de rescate y está conectado al móvil.",
                "It shows up once the RP2040 runs the rescue firmware and is plugged into the phone."), 13, false);
        button(c5, t("Compartir registro", "Share the log")).setOnClickListener(v -> shareLog());
        button(c5, t("Reiniciar la señal de arranque", "Restart the boot signal")).setOnClickListener(v -> {
            pendingCmd = "rekey";
            logLine(t("Reiniciando la señal: apaga el FM-1, espera 3 s y enciéndelo cuando lo pida la app.",
                    "Restarting the signal: switch the FM-1 off, wait 3 s and switch it on when the app says so."));
        });

        log = new TextView(this);
        log.setTypeface(Typeface.MONOSPACE);
        log.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        log.setTextColor(0xFF6F6E69);
        root.addView(log);

        TextView foot = text(root, t("Solo se escribe la zona del firmware del FM-1 (0x4000–0x92FFF), cada sector verificado; "
                + "tus ajustes, sonidos y canciones no se tocan. Herramienta no oficial: úsala con la batería cargada.",
                "Only the FM-1's firmware area (0x4000–0x92FFF) is written, each sector verified; your settings, "
                + "sounds and songs are not touched. Unofficial tool: use it on a charged battery."), 12, false);
        foot.setTextColor(0xFF6F6E69);
        foot.setGravity(Gravity.START);
        setContentView(scroll);
        show(t("Conecta el RP2040", "Plug in the RP2040"), "", 0);
    }

    private void show(String s, String d, int permille) {
        ui.post(() -> {
            stage.setText(s);
            detail.setText(d);
            progress.setProgress(permille);
        });
    }

    private void logLine(String s) {
        synchronized (fullLog) {
            fullLog.append(new java.text.SimpleDateFormat("HH:mm:ss", Locale.ROOT).format(new java.util.Date()))
                   .append("  ").append(s).append('\n');
            if (fullLog.length() > 400_000) fullLog.delete(0, fullLog.length() - 300_000);
        }
        ui.post(() -> {
            String all = log.getText() + s + "\n";
            if (all.length() > 4000) all = all.substring(all.length() - 4000);
            log.setText(all);
        });
    }

    private String board() { return prefs.getString("board", "zero"); }

    /** Console line from the RP2040: into the full log, the diagnosis, and (unless noise) the screen. */
    private void consoleLine(String line) {
        Diagnosis d = diag;
        d.feed(line);
        synchronized (fullLog) {
            fullLog.append("RP2040 ").append(line).append('\n');
            if (fullLog.length() > 400_000) fullLog.delete(0, fullLog.length() - 300_000);
        }
        if (!Diagnosis.noise(line) && !line.contains("KEY: ") ) {
            ui.post(() -> {
                String all = log.getText() + "› " + line + "\n";
                if (all.length() > 4000) all = all.substring(all.length() - 4000);
                log.setText(all);
            });
        }
        String[] sum = d.summary(es);
        ui.post(() -> {
            diagHead.setText(sum[0]);
            diagText.setText(sum[1]);
        });
    }

    private void shareLog() {
        String text;
        synchronized (fullLog) { text = fullLog.toString(); }
        String[] sum = diag.summary(es);
        String head = "FM-1 Rescue " + board() + " · " + installedName() + "\n" + sum[0] + "\n" + sum[1] + "\n\n";
        Intent i = new Intent(Intent.ACTION_SEND);
        i.setType("text/plain");
        i.putExtra(Intent.EXTRA_SUBJECT, "FM-1 Rescue log");
        String body = head + text;
        if (body.length() > 90_000) body = head + "…\n" + text.substring(text.length() - 90_000 + head.length());
        i.putExtra(Intent.EXTRA_TEXT, body);
        startActivity(Intent.createChooser(i, t("Compartir registro", "Share the log")));
    }

    // ------------------------------------------------------------ files

    private File v15File() { return new File(getFilesDir(), "FM-1-V15.fwsc"); }
    private File customFile() { return new File(getFilesDir(), "custom.fwsc"); }

    private String source() { return prefs.getString("src", "jiant"); }

    /** The package to restore, or null while it is not available. */
    private byte[] selected() {
        switch (source()) {
            case "v15": return v15Pkg;
            case "custom": return customPkg;
            default: return jiantPkg;
        }
    }

    /** What the board logs and the payload carries. */
    private String label() {
        switch (source()) {
            case "v15": return "FM-1_015 official V15";
            case "custom": return customLabel;
            default: return "JIANT " + jiantVersion;
        }
    }

    private String installedName() {
        switch (source()) {
            case "v15": return "V15";
            case "custom": return customLabel;
            default: return "JIANT " + jiantVersion;
        }
    }

    private void loadFiles() {
        // The public JIANT, put in by android/rescue/fetch_firmware.py when the APK is built.
        try (InputStream in = getAssets().open("jiant.fwsc"); InputStream js = getAssets().open("jiant.json")) {
            byte[] d = readAll(in);
            RescueImage.checkFirmware(d);
            String meta = new String(readAll(js), StandardCharsets.UTF_8);
            java.util.regex.Matcher m = java.util.regex.Pattern.compile("\"version\"\\s*:\\s*\"([^\"]*)\"").matcher(meta);
            jiantVersion = m.find() ? m.group(1) : "";
            jiantPkg = d;
        } catch (Exception e) {
            jiantPkg = null;
            logLine("JIANT: " + e.getMessage());
        }
        // The official V15: inside the APK (fetch_firmware.py), else the copy downloaded or chosen earlier.
        try (InputStream in = getAssets().open("FM-1-V15.fwsc")) {
            byte[] d = readAll(in);
            RescueImage.checkV15(d);
            v15Pkg = d;
        } catch (Exception e) { v15Pkg = null; }
        try {
            if (v15Pkg == null && v15File().exists()) {
                byte[] d = Files.readAllBytes(v15File().toPath());
                RescueImage.checkV15(d);
                v15Pkg = d;
            }
        } catch (Exception e) { v15Pkg = null; }
        try {
            if (customFile().exists()) {
                byte[] d = Files.readAllBytes(customFile().toPath());
                RescueImage.Firmware f = RescueImage.checkFirmware(d);
                customLabel = prefs.getString("customLabel", f.identity);
                customPkg = d;
            }
        } catch (Exception e) { customPkg = null; }
        // jl-uboot-tool's loader ships in the APK (MIT, assets/wl82loader-LICENSE.txt).
        try (InputStream in = getAssets().open("wl82loader.bin")) {
            loader = RescueImage.checkLoader(readAll(in));
        } catch (Exception e) {
            loader = null;
            logLine("loader: " + e.getMessage());
        }
        if (jiantPkg == null && source().equals("jiant")) {
            prefs.edit().putString("src", "v15").apply();
            ui.post(() -> srcV15.setChecked(true));
        }
        updateFileTexts();
        if (v15Pkg == null && source().equals("v15")) net.execute(this::downloadV15);
    }

    private volatile boolean downloading;

    private void updateFileTexts() {
        ui.post(() -> {
            srcJiant.setText(jiantPkg != null
                    ? t("JIANT " + jiantVersion + "  (incluido · recomendado: sirve para FM-1 que venían en V15 o en V16)",
                        "JIANT " + jiantVersion + "  (included · recommended: works on FM-1s that came with V15 or V16)")
                    : t("JIANT (no incluido en esta compilación)", "JIANT (not included in this build)"));
            srcJiant.setEnabled(jiantPkg != null);
            srcV15.setText(v15Pkg != null || !downloading
                    ? t("V15 oficial de M-VAVE  (incluida)", "Official M-VAVE V15  (included)")
                    : t("V15 oficial de M-VAVE  (descargando…)", "Official M-VAVE V15  (downloading…)"));
            srcCustom.setText(t("Mi archivo: ", "My file: ") + customLabel);
            srcCustom.setVisibility(customPkg != null ? View.VISIBLE : View.GONE);

            String st;
            int color = 0xFF2E8B57;
            if (selected() != null) {
                st = t("✓ Listo para instalar: ", "✓ Ready to install: ") + installedName();
                if (source().equals("v15")) st += t(". Ojo: si tu FM-1 venía en V16, usa JIANT.",
                        ". Note: if your FM-1 came with V16, use JIANT.");
            } else if (source().equals("v15")) {
                st = downloading ? t("Descargando la V15 oficial…", "Downloading the official V15…")
                        : t("No se pudo descargar la V15 (¿sin internet?). Se reintenta sola.",
                            "Could not download V15 (no internet?). It retries by itself.");
                color = 0xFF1D1D1B;
            } else {
                st = t("No disponible.", "Not available.");
                color = 0xFFC0392B;
            }
            fwscText.setText(st);
            fwscText.setTextColor(color);
            loaderText.setText(loader != null ? t("✓ Loader de JieLi incluido", "✓ JieLi loader included")
                    : t("✗ El loader incluido está dañado: reinstala la app.", "✗ The bundled loader is damaged: reinstall the app."));
        });
    }

    /** The official V15, downloaded once and kept; accepted only if its firmware region hash is V15's. */
    private void downloadV15() {
        if (v15Pkg != null || downloading) return;
        downloading = true;
        updateFileTexts();
        try {
            HttpURLConnection c = (HttpURLConnection) new URL(RescueImage.V15_URL).openConnection();
            c.setConnectTimeout(15000);
            c.setReadTimeout(30000);
            byte[] d;
            try (InputStream in = c.getInputStream()) { d = readAll(in); }
            RescueImage.checkV15(d);
            try (FileOutputStream o = new FileOutputStream(v15File())) { o.write(d); }
            v15Pkg = d;
            logLine(t("V15 descargada y verificada", "V15 downloaded and verified"));
            downloading = false;
            updateFileTexts();
        } catch (Exception e) {
            downloading = false;
            logLine("V15: " + e.getMessage());
            updateFileTexts();
            if (source().equals("v15")) ui.postDelayed(() -> net.execute(this::downloadV15), 15000);
        }
    }

    private static byte[] readAll(InputStream in) throws IOException {
        ByteArrayOutputStream o = new ByteArrayOutputStream();
        byte[] buf = new byte[16384];
        for (int n; (n = in.read(buf)) > 0; ) o.write(buf, 0, n);
        return o.toByteArray();
    }

    @Override
    protected void onActivityResult(int req, int res, Intent data) {
        super.onActivityResult(req, res, data);
        if (req != PICK_FWSC || res != RESULT_OK || data == null) return;
        Uri u = data.getData();
        try (InputStream in = getContentResolver().openInputStream(u)) {
            byte[] d = readAll(in);
            RescueImage.Firmware f = RescueImage.checkFirmware(d);
            if (f.official) {
                try (FileOutputStream o = new FileOutputStream(v15File())) { o.write(d); }
                v15Pkg = d;
                prefs.edit().putString("src", "v15").apply();
                ui.post(() -> srcV15.setChecked(true));
                logLine("V15 OK");
            } else {
                try (FileOutputStream o = new FileOutputStream(customFile())) { o.write(d); }
                customPkg = d;
                String name = u.getLastPathSegment() == null ? "" : u.getLastPathSegment();
                name = name.substring(name.lastIndexOf('/') + 1).replaceAll("\\.fwsc$", "");
                customLabel = (name.isEmpty() ? f.identity : name + " (" + f.identity + ")");
                if (customLabel.length() > 60) customLabel = customLabel.substring(0, 60);
                prefs.edit().putString("src", "custom").putString("customLabel", customLabel).apply();
                ui.post(() -> srcCustom.setChecked(true));
                logLine(t("Paquete aceptado: ", "Package accepted: ") + customLabel);
            }
        } catch (RescueImage.Refused e) {
            logLine(t("Rechazado: ", "Refused: ") + e.getMessage());
            String why = e.getMessage().contains("FM-1_016")
                    ? t("Esto es la V16 oficial: no cabe en la zona que escribe el rescate. Usa JIANT; después, si quieres, "
                        + "instala la V16 con \"Volver al oficial\" del instalador web.",
                        "This is the official V16: it does not fit the area the rescue writes. Use JIANT; then, if you "
                        + "like, install V16 with the web installer's \"Return to official\".")
                    : e.getMessage();
            ui.post(() -> {
                fwscText.setText(t("✗ Rechazado: ", "✗ Refused: ") + why);
                fwscText.setTextColor(0xFFC0392B);
            });
            return;
        } catch (IOException e) {
            logLine("read: " + e.getMessage());
            return;
        }
        updateFileTexts();
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);          // USB_DEVICE_ATTACHED: the poll picks the board up
    }

    @Override
    protected void onDestroy() {
        stopMonitor = true;
        ui.removeCallbacksAndMessages(null);
        usb.shutdownNow();
        net.shutdownNow();
        super.onDestroy();
    }

    // ------------------------------------------------------------ USB

    private UsbDevice find(int pid) {
        for (UsbDevice d : usbManager.getDeviceList().values()) {
            if (d.getVendorId() == Picoboot.VID && d.getProductId() == pid) return d;
        }
        return null;
    }

    private boolean permitted(UsbDevice d) {
        if (usbManager.hasPermission(d)) return true;
        if (asked.add(d.getDeviceName())) {
            int flags = Build.VERSION.SDK_INT >= 31 ? PendingIntent.FLAG_MUTABLE : 0;
            Intent i = new Intent(ACTION_PERMISSION).setPackage(getPackageName());
            usbManager.requestPermission(d, PendingIntent.getBroadcast(this, 0, i, flags));
        }
        show(t("Permite el acceso USB", "Allow USB access"),
                t("Acepta el aviso de Android (marca \"usar siempre\" para no volver a verlo).",
                  "Accept Android's prompt (tick \"always\" not to see it again)."), 0);
        return false;
    }

    private final Runnable poll = new Runnable() {
        @Override public void run() {
            try { tick(); } catch (Exception e) { logLine("poll: " + e); }
            ui.postDelayed(this, 700);
        }
    };

    private void tick() {
        if (busy) return;
        UsbDevice boot = find(Picoboot.PID_BOOTSEL), fw = find(PID_RESCUE);
        if (monitoring != null && (fw == null || !fw.getDeviceName().equals(monitoring))) stopMonitor = true;
        reflash.setVisibility(fw != null && monitoring != null ? View.VISIBLE : View.GONE);

        if (boot != null) {
            if (selected() == null || loader == null) {
                show(t("RP2040 conectado", "RP2040 connected"),
                        selected() == null ? t("Esperando el firmware del paso 1 (la V15 necesita internet la primera vez).",
                                         "Waiting for the firmware of step 1 (V15 needs the internet the first time).")
                                     : t("El loader incluido está dañado: reinstala la app.", "The bundled loader is damaged: reinstall the app."), 0);
                return;
            }
            if (!permitted(boot)) return;
            busy = true;
            usb.execute(() -> program(boot));
            return;
        }
        if (fw != null) {
            if (monitoring == null) {
                if (!permitted(fw)) return;
                monitoring = fw.getDeviceName();
                stopMonitor = false;
                usb.execute(() -> monitor(fw));
            }
            return;
        }
        UsbDevice other = otherRp2040();
        if (other != null) {
            // An RP2040 with some other firmware (MicroPython, a demo, a Pico SDK program): send it to BOOTSEL
            // ourselves, once per plug-in, so BOOT does not have to be held.
            if (!permitted(other)) return;
            if (kicked.add(other.getDeviceName())) {
                show(t("RP2040 con otro programa", "RP2040 with another program"),
                        t("Pasándolo a modo de grabación…", "Switching it to programming mode…"), 0);
                usb.execute(() -> kickToBootsel(other));
            } else {
                show(t("No responde al reinicio", "It does not answer the reset"),
                        t("Desconéctalo, mantén BOOT pulsado y vuelve a enchufarlo.",
                          "Unplug it, hold BOOT and plug it in again."), 0);
            }
            return;
        }
        if (monitoring == null) {
            show(t("Conecta el RP2040", "Plug in the RP2040"),
                    t("Enchúfalo al móvil con un adaptador OTG. Si la app no lo detecta, desconéctalo y vuelve a "
                      + "enchufarlo manteniendo BOOT pulsado.",
                      "Plug it into the phone with an OTG adapter. If the app does not see it, unplug it and plug "
                      + "it in again holding BOOT."), 0);
        }
    }

    private final Set<String> kicked = new HashSet<>();

    /** A Raspberry Pi VID device that is neither the boot ROM nor the rescue firmware. */
    private UsbDevice otherRp2040() {
        for (UsbDevice d : usbManager.getDeviceList().values()) {
            if (d.getVendorId() == Picoboot.VID && d.getProductId() != Picoboot.PID_BOOTSEL
                    && d.getProductId() != PID_RESCUE) return d;
        }
        return null;
    }

    /**
     * Reboots a running RP2040 into BOOTSEL: the Raspberry Pi reset interface (Pico SDK programs,
     * picotool's way), else the "1200 baud touch" on each CDC port (MicroPython, Arduino, Pico SDK).
     * Runs on the USB thread; the board drops off the bus if it worked.
     */
    private void kickToBootsel(UsbDevice d) {
        UsbDeviceConnection c = usbManager.openDevice(d);
        if (c == null) return;
        try {
            boolean done = false;
            for (int i = 0; i < d.getInterfaceCount() && !done; i++) {
                UsbInterface f = d.getInterface(i);
                if (f.getInterfaceClass() == UsbConstants.USB_CLASS_VENDOR_SPEC && f.getInterfaceSubclass() == 0
                        && f.getInterfaceProtocol() == 1) {
                    c.claimInterface(f, true);
                    logLine(t("Reinicio a BOOT por la interfaz de reset", "Reset to BOOT via the reset interface"));
                    c.controlTransfer(0x41, 0x01, 0, f.getId(), null, 0, 1000);     // RESET_REQUEST_BOOTSEL
                    done = true;
                }
            }
            for (int i = 0; i < d.getInterfaceCount() && !done; i++) {
                UsbInterface f = d.getInterface(i);
                if (f.getInterfaceClass() != UsbConstants.USB_CLASS_COMM) continue;
                c.claimInterface(f, true);
                byte[] coding = {(byte) 0xB0, 0x04, 0, 0, 0, 0, 8};                  // 1200 baud, 8N1
                logLine(t("Reinicio a BOOT con 1200 baudios (puerto ", "Reset to BOOT with 1200 baud (port ")
                        + f.getId() + ")");
                c.controlTransfer(0x21, 0x22, 0x0003, f.getId(), null, 0, 500);     // DTR + RTS on
                c.controlTransfer(0x21, 0x20, 0, f.getId(), coding, coding.length, 500);   // SET_LINE_CODING
                c.controlTransfer(0x21, 0x22, 0x0000, f.getId(), null, 0, 500);     // DTR off: the "touch"
            }
        } catch (Exception e) {
            logLine("reset: " + e.getMessage());
        } finally {
            c.close();
        }
        sleep(2500);
    }

    /** Programs the board in BOOTSEL over PICOBOOT. Runs on the USB thread. */
    private void program(UsbDevice d) {
        UsbDeviceConnection conn = null;
        try {
            show(t("Preparando…", "Preparing…"), "", 0);
            byte[] base = readAll(getAssets().open("fm1-rescue-base-" + board() + ".uf2"));
            byte[] pkg = selected();
            if (pkg == null) throw new IOException(t("no hay firmware elegido", "no firmware chosen"));
            TreeMap<Integer, byte[]> plan = RescueImage.build(base, pkg, label(), loader);
            prefs.edit().putString("lastInstalled", installedName()).apply();

            UsbInterface itf = null;
            UsbEndpoint in = null, out = null;
            for (int i = 0; i < d.getInterfaceCount() && itf == null; i++) {
                UsbInterface f = d.getInterface(i);
                if (f.getInterfaceClass() != UsbConstants.USB_CLASS_VENDOR_SPEC) continue;
                for (int e = 0; e < f.getEndpointCount(); e++) {
                    UsbEndpoint ep = f.getEndpoint(e);
                    if (ep.getType() != UsbConstants.USB_ENDPOINT_XFER_BULK) continue;
                    if (ep.getDirection() == UsbConstants.USB_DIR_IN) in = ep; else out = ep;
                }
                if (in != null && out != null) itf = f;
            }
            if (itf == null) throw new IOException("no PICOBOOT interface");
            conn = usbManager.openDevice(d);
            if (conn == null || !conn.claimInterface(itf, true)) throw new IOException("cannot open the RP2040");
            final UsbDeviceConnection c = conn;
            final UsbEndpoint fin = in, fout = out;
            final int itfId = itf.getId();
            Picoboot pb = new Picoboot(new Picoboot.Transport() {
                public int bulkOut(byte[] b, int len, int to) { return c.bulkTransfer(fout, b, len, to); }
                public int bulkIn(byte[] b, int len, int to) { return c.bulkTransfer(fin, b, len, to); }
                public int controlOut(int req, int to) { return c.controlTransfer(0x41, req, 0, itfId, null, 0, to); }
            });
            logLine(t("Grabando ", "Programming ") + plan.size() + t(" sectores (", " sectors (") + board() + ")");
            pb.program(plan, (what, n, total) -> {
                boolean w = what.equals("write");
                show(t("Grabando el RP2040…", "Programming the RP2040…"),
                        (w ? t("Escribiendo ", "Writing ") : t("Comprobando ", "Checking ")) + n + " / " + total,
                        w ? n * 700 / total : 700 + n * 300 / total);
            });
            logLine(t("RP2040 grabado y verificado; reiniciando", "RP2040 programmed and verified; rebooting"));
            show(t("RP2040 listo", "RP2040 ready"), t("Reiniciando…", "Rebooting…"), 1000);
        } catch (Exception e) {
            logLine("ERROR: " + e.getMessage());
            show(t("No se pudo grabar el RP2040", "Could not program the RP2040"),
                    e.getMessage() + t("\nDesconéctalo y vuelve a conectarlo con BOOT pulsado.",
                            "\nUnplug it and plug it in again with BOOT held."), 0);
            sleep(4000);
        } finally {
            if (conn != null) conn.close();
            sleep(1500);
            busy = false;
        }
    }

    /** Sends a board running the rescue firmware back to BOOTSEL (Raspberry Pi reset interface). */
    private void resetToBootsel(UsbDevice d) {
        stopMonitor = true;
        usb.execute(() -> {
            UsbDeviceConnection c = usbManager.openDevice(d);
            if (c == null) return;
            try {
                for (int i = 0; i < d.getInterfaceCount(); i++) {
                    UsbInterface f = d.getInterface(i);
                    if (f.getInterfaceClass() == UsbConstants.USB_CLASS_VENDOR_SPEC && f.getInterfaceProtocol() == 1) {
                        c.claimInterface(f, true);
                        logLine(t("Pasando el RP2040 a modo BOOT", "Sending the RP2040 to BOOT mode"));
                        c.controlTransfer(0x41, 0x01, 0, f.getId(), null, 0, 1000);   // RESET_REQUEST_BOOTSEL
                    }
                }
            } finally {
                c.close();
            }
        });
    }

    /** Reads the rescue progress from the board's data port. Runs on the USB thread until detach. */
    private void monitor(UsbDevice d) {
        UsbDeviceConnection c = usbManager.openDevice(d);
        try {
            if (c == null) throw new IOException("cannot open the board");
            UsbInterface data = null;
            UsbEndpoint in = null, out = null;
            for (int i = 0; i < d.getInterfaceCount(); i++) {
                UsbInterface f = d.getInterface(i);
                if (f.getId() == 3 && f.getInterfaceClass() == UsbConstants.USB_CLASS_CDC_DATA) data = f;
            }
            if (data == null) throw new IOException("no data port");
            for (int e = 0; e < data.getEndpointCount(); e++) {
                UsbEndpoint ep = data.getEndpoint(e);
                if (ep.getDirection() == UsbConstants.USB_DIR_IN) in = ep; else out = ep;
            }
            if (!c.claimInterface(data, true)) throw new IOException("data port busy");
            c.controlTransfer(0x21, 0x22, 0x0001, 2, null, 0, 1000);     // DTR on CDC 1 (interface 2)

            // The console (CDC 0, data interface 1): the Transporter's log, read all the time (an
            // unread console can stall it) and turned into the diagnosis.
            UsbEndpoint cin = null;
            for (int i = 0; i < d.getInterfaceCount(); i++) {
                UsbInterface f = d.getInterface(i);
                if (f.getId() == 1 && f.getInterfaceClass() == UsbConstants.USB_CLASS_CDC_DATA && c.claimInterface(f, true)) {
                    for (int e = 0; e < f.getEndpointCount(); e++) {
                        UsbEndpoint ep = f.getEndpoint(e);
                        if (ep.getDirection() == UsbConstants.USB_DIR_IN && ep.getType() == UsbConstants.USB_ENDPOINT_XFER_BULK) cin = ep;
                    }
                }
            }
            diag = new Diagnosis();
            if (cin != null) c.controlTransfer(0x21, 0x22, 0x0001, 0, null, 0, 1000);   // DTR on CDC 0
            else logLine(t("Sin consola del RP2040: no habrá diagnóstico", "No RP2040 console: no diagnostics"));

            byte[] req = "rescue\n".getBytes(StandardCharsets.US_ASCII);
            byte[] buf = new byte[512], cbuf = new byte[512];
            StringBuilder pending = new StringBuilder(), cpending = new StringBuilder();
            long nextReq = 0, lastStatus = System.currentTimeMillis();
            boolean warned = false;
            while (!stopMonitor) {
                long now = System.currentTimeMillis();
                String cmd = pendingCmd;
                if (cmd != null) {
                    pendingCmd = null;
                    byte[] cb = (cmd + "\n").getBytes(StandardCharsets.US_ASCII);
                    c.bulkTransfer(out, cb, cb.length, 500);
                    if (cmd.equals("rekey")) diag = new Diagnosis();
                }
                if (now >= nextReq) {
                    c.bulkTransfer(out, req, req.length, 300);
                    nextReq = now + 700;
                }
                if (cin != null) {
                    int n = c.bulkTransfer(cin, cbuf, cbuf.length, 40);
                    if (n > 0) {
                        cpending.append(new String(cbuf, 0, n, StandardCharsets.ISO_8859_1));
                        int nl;
                        while ((nl = cpending.indexOf("\n")) >= 0) {
                            String line = cpending.substring(0, nl).replace("\r", "");
                            cpending.delete(0, nl + 1);
                            consoleLine(line);
                        }
                        if (cpending.length() > 4096) cpending.setLength(0);
                    }
                }
                int n = c.bulkTransfer(in, buf, buf.length, 40);
                if (n > 0) {
                    pending.append(new String(buf, 0, n, StandardCharsets.US_ASCII));
                    int nl;
                    while ((nl = pending.indexOf("\n")) >= 0) {
                        RescueStatus st = RescueStatus.parse(pending.substring(0, nl));
                        pending.delete(0, nl + 1);
                        if (st != null) {
                            lastStatus = now;
                            warned = false;
                            report(d, st);
                        }
                    }
                    if (pending.length() > 4096) pending.setLength(0);
                }
                if (!warned && now - lastStatus > 5000) {
                    warned = true;
                    show(t("La placa no responde", "The board does not answer"),
                            t("¿Es una placa con el firmware FM-1 Rescue? Desconéctala y vuelve a conectarla.",
                              "Is it a board with the FM-1 Rescue firmware? Unplug and re-plug it."), 0);
                }
            }
        } catch (Exception e) {
            logLine("monitor: " + e.getMessage());
            sleep(1500);
        } finally {
            if (c != null) c.close();
            monitoring = null;
        }
    }

    private String lastState = "";
    private long offSince;

    /** The board's identity for "once per board": its serial number (unique RP2040 ID), else the port. */
    private String boardKey(UsbDevice d) {
        try {
            String sn = d.getSerialNumber();
            if (sn != null && !sn.isEmpty()) return sn;
        } catch (SecurityException e) { /* no permission yet */ }
        return d.getDeviceName();
    }

    private void report(UsbDevice d, RescueStatus s) {
        if (!s.state.equals(lastState)) {
            logLine("FM-1 Rescue: " + s.state + (s.err.equals("none") ? "" : " (" + s.err + " @ " + s.addr + ")"));
            lastState = s.state;
        }
        if (!s.state.equals("off")) offSince = 0;
        switch (s.state) {
            case "boot":
                show(t("Iniciando el RP2040…", "Starting the RP2040…"), "", 0);
                break;
            case "off":
                // "off" for a few seconds in a row: a plain Transporter (no payload). Older rescue firmware
                // also said "off" for its first seconds, so a single "off" proves nothing.
                if (offSince == 0) offSince = System.currentTimeMillis();
                if (System.currentTimeMillis() - offSince < 4000) {
                    show(t("Comprobando el RP2040…", "Checking the RP2040…"), "", 0);
                    break;
                }
                // fall through
            case "bad-payload":
                // A plain Transporter, or a damaged payload: reprogram it, once per board.
                if (selected() != null && loader != null && autoReset.add(boardKey(d))) {
                    show(t("Actualizando el RP2040…", "Updating the RP2040…"), "", 0);
                    resetToBootsel(d);
                } else {
                    show(t("El RP2040 no tiene la V15 dentro", "The RP2040 has no V15 inside"),
                            t("Espera a que la V15 esté lista y pulsa \"Volver a grabar el RP2040\".",
                              "Wait for V15 to be ready and press \"Reprogram the RP2040\"."), 0);
                }
                break;
            case "wait":
                if (s.foreign) {
                    show(t("Apaga el FM-1", "Switch the FM-1 off"),
                            t("Está encendido con otro firmware. Apágalo, desconecta y vuelve a conectar el RP2040, y enciéndelo.",
                              "It is running another firmware. Switch it off, re-plug the RP2040, switch it on."), 0);
                } else {
                    show(t("Enciende el FM-1 ahora", "Switch the FM-1 on now"),
                            board().equals("pico")
                                    ? t("Tiene que haber estado apagado. El LED de la Pico parpadea lento.",
                                        "It must have been off. The Pico's LED blinks slowly.")
                                    : t("Tiene que haber estado apagado. El LED del RP2040 parpadea en amarillo.",
                                        "It must have been off. The RP2040's LED blinks yellow."), 0);
                }
                break;
            case "run":
                String ph = s.phase.equals("verify") ? t("comprobando", "checking")
                        : s.phase.equals("write") ? t("escribiendo", "writing") : t("conectando", "connecting");
                show(t("Reparando el FM-1…", "Repairing the FM-1…"),
                        ph + "  " + s.sector + " / " + s.sectors + t("   (no desconectes nada)", "   (do not unplug anything)"),
                        s.permille());
                break;
            case "done":
                show(t("¡Listo! ✓", "Done! ✓"),
                        t("Apaga el FM-1, quita los cables y vuelve a encenderlo: arranca con "
                          + prefs.getString("lastInstalled", "el firmware instalado") + ". Los datos guardados (canciones, ajustes) no se tocaron.",
                          "Switch the FM-1 off, remove the wires and switch it on: it boots "
                          + prefs.getString("lastInstalled", "the installed firmware") + ". Saved data (songs, settings) was not touched."), 1000);
                break;
            case "error":
                show(t("Error", "Error"), errorText(s), 0);
                break;
            default:
                show(s.state, "", 0);
        }
    }

    private String errorText(RescueStatus s) {
        String again = t("Apaga el FM-1, desconecta y vuelve a conectar el RP2040, y enciende el FM-1 otra vez.",
                "Switch the FM-1 off, unplug and re-plug the RP2040, switch the FM-1 on again.");
        switch (s.err) {
            case "chip": return t("Esto no parece un FM-1 (chip o memoria distintos). No se escribió nada.",
                    "This does not look like an FM-1 (other chip or flash). Nothing was written.");
            case "link": return t("Se perdió la conexión antes de escribir nada. Revisa los cables (prueba a cambiar verde y blanco). ",
                    "The connection was lost before anything was written. Check the wires (try swapping green and white). ") + again;
            case "write": return t("Una escritura no se completó en " + s.addr + ": el firmware quedó a medias. Repite: ",
                    "A write did not complete at " + s.addr + ": the firmware is incomplete. Repeat: ") + again;
            default: return t("La comprobación final no coincide. Repite: ", "The final check does not match. Repeat: ") + again;
        }
    }

    private static void sleep(long ms) {
        try { Thread.sleep(ms); } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
    }
}
