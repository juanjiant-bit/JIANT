// SPDX-License-Identifier: MIT
// FM-1 Rescue for Android: one screen that does the whole unbrick.
//   1. The official V15 FM-1.fwsc is chosen once (hash-checked, kept in the
//      app); JieLi's wl82loader.bin is downloaded once (hash-checked).
//   2. The RP2040 (RP2040-Zero or XIAO) is plugged into the phone with an OTG
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

    private TextView fwscText, loaderText, stage, detail, log;
    private ProgressBar progress;
    private Button reflash;

    private byte[] fwsc, loader;
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
        text(c1, t("1 · Firmware oficial V15", "1 · Official V15 firmware"), 16, true);
        fwscText = text(c1, "", 14, false);
        button(c1, t("Elegir FM-1.fwsc", "Choose FM-1.fwsc")).setOnClickListener(v -> {
            Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            i.addCategory(Intent.CATEGORY_OPENABLE);
            i.setType("*/*");
            startActivityForResult(i, PICK_FWSC);
        });
        loaderText = text(c1, "", 13, false);

        LinearLayout c2 = card(root);
        text(c2, t("2 · Tu placa", "2 · Your board"), 16, true);
        RadioGroup g = new RadioGroup(this);
        RadioButton zero = new RadioButton(this), xiao = new RadioButton(this);
        zero.setText(t("Waveshare RP2040-Zero  (verde → 0, blanco → 1, negro → GND)",
                "Waveshare RP2040-Zero  (green → 0, white → 1, black → GND)"));
        xiao.setText(t("Seeed XIAO RP2040  (verde → D6, blanco → D7, negro → GND)",
                "Seeed XIAO RP2040  (green → D6, white → D7, black → GND)"));
        zero.setId(View.generateViewId());
        xiao.setId(View.generateViewId());
        g.addView(zero);
        g.addView(xiao);
        (board().equals("xiao") ? xiao : zero).setChecked(true);
        g.setOnCheckedChangeListener((gr, id) -> prefs.edit().putString("board", id == xiao.getId() ? "xiao" : "zero").apply());
        c2.addView(g);

        LinearLayout c3 = card(root);
        text(c3, t("3 · Conectar", "3 · Connect"), 16, true);
        text(c3, t("• FM-1 APAGADO. Cable rojo sin conectar.\n"
                + "• Conecta los 3 cables del RP2040 al USB-C del FM-1.\n"
                + "• Mantén BOOT en el RP2040 y enchúfalo a este móvil con un adaptador OTG.\n"
                + "• La app hace el resto y te dice cuándo encender el FM-1.",
                "• FM-1 OFF. Red wire unconnected.\n"
                + "• Connect the RP2040's 3 wires to the FM-1's USB-C.\n"
                + "• Hold BOOT on the RP2040 and plug it into this phone with an OTG adapter.\n"
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
        ui.post(() -> {
            String all = log.getText() + s + "\n";
            if (all.length() > 4000) all = all.substring(all.length() - 4000);
            log.setText(all);
        });
    }

    private String board() { return prefs.getString("board", "zero"); }

    // ------------------------------------------------------------ files

    private File fwscFile() { return new File(getFilesDir(), "FM-1-V15.fwsc"); }
    private File loaderFile() { return new File(getFilesDir(), "wl82loader.bin"); }

    private void loadFiles() {
        try {
            if (fwscFile().exists()) {
                byte[] d = Files.readAllBytes(fwscFile().toPath());
                RescueImage.checkV15(d);
                fwsc = d;
            }
        } catch (Exception e) { fwsc = null; }
        try {
            if (loaderFile().exists()) loader = RescueImage.checkLoader(Files.readAllBytes(loaderFile().toPath()));
        } catch (Exception e) { loader = null; }
        updateFileTexts();
        if (loader == null) net.execute(this::downloadLoader);
    }

    private void updateFileTexts() {
        ui.post(() -> {
            fwscText.setText(fwsc != null ? t("✓ V15 oficial verificada", "✓ Official V15 verified")
                    : t("Elige el FM-1.fwsc de la V15 de M-VAVE (699.956 bytes). Se comprueba por su hash.",
                        "Choose M-VAVE's V15 FM-1.fwsc (699,956 bytes). It is checked by its hash."));
            fwscText.setTextColor(fwsc != null ? 0xFF2E8B57 : 0xFF1D1D1B);
            loaderText.setText(loader != null ? t("✓ Loader de JieLi listo", "✓ JieLi loader ready")
                    : t("Descargando el loader de JieLi (jl-uboot-tool)…", "Downloading JieLi's loader (jl-uboot-tool)…"));
        });
    }

    private void downloadLoader() {
        try {
            HttpURLConnection c = (HttpURLConnection) new URL(RescueImage.LOADER_URL).openConnection();
            c.setConnectTimeout(15000);
            c.setReadTimeout(15000);
            byte[] d;
            try (InputStream in = c.getInputStream()) { d = readAll(in); }
            RescueImage.checkLoader(d);
            try (FileOutputStream o = new FileOutputStream(loaderFile())) { o.write(d); }
            loader = d;
            updateFileTexts();
        } catch (Exception e) {
            ui.post(() -> loaderText.setText(t("No se pudo descargar el loader (¿sin internet?). Se reintenta…",
                    "Could not download the loader (no internet?). Retrying…")));
            ui.postDelayed(() -> net.execute(this::downloadLoader), 10000);
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
            RescueImage.checkV15(d);
            try (FileOutputStream o = new FileOutputStream(fwscFile())) { o.write(d); }
            fwsc = d;
            logLine("V15 OK");
        } catch (RescueImage.Refused e) {
            logLine(t("Rechazado: ", "Refused: ") + e.getMessage());
            ui.post(() -> {
                fwscText.setText(t("✗ No es la V15 oficial. ", "✗ Not the official V15. ") + e.getMessage());
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
            if (fwsc == null || loader == null) {
                show(t("RP2040 conectado", "RP2040 connected"),
                        fwsc == null ? t("Elige primero el FM-1.fwsc (paso 1).", "Choose the FM-1.fwsc first (step 1).")
                                     : t("Esperando el loader de JieLi…", "Waiting for JieLi's loader…"), 0);
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
        if (monitoring == null) {
            show(t("Conecta el RP2040", "Plug in the RP2040"),
                    t("Mantén BOOT en el RP2040 y enchúfalo al móvil con un adaptador OTG.",
                      "Hold BOOT on the RP2040 and plug it into the phone with an OTG adapter."), 0);
        }
    }

    /** Programs the board in BOOTSEL over PICOBOOT. Runs on the USB thread. */
    private void program(UsbDevice d) {
        UsbDeviceConnection conn = null;
        try {
            show(t("Preparando…", "Preparing…"), "", 0);
            byte[] base = readAll(getAssets().open("fm1-rescue-base-" + board() + ".uf2"));
            TreeMap<Integer, byte[]> plan = RescueImage.build(base, fwsc, loader);

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
            byte[] req = "rescue\n".getBytes(StandardCharsets.US_ASCII);
            byte[] buf = new byte[256];
            StringBuilder pending = new StringBuilder();
            int silent = 0;
            while (!stopMonitor) {
                c.bulkTransfer(out, req, req.length, 500);
                long until = System.currentTimeMillis() + 500;
                RescueStatus st = null;
                while (System.currentTimeMillis() < until && st == null) {
                    int n = c.bulkTransfer(in, buf, buf.length, 200);
                    if (n <= 0) continue;
                    pending.append(new String(buf, 0, n, StandardCharsets.US_ASCII));
                    int nl;
                    while ((nl = pending.indexOf("\n")) >= 0) {
                        RescueStatus s = RescueStatus.parse(pending.substring(0, nl));
                        pending.delete(0, nl + 1);
                        if (s != null) st = s;
                    }
                }
                if (st == null) {
                    if (++silent == 6) show(t("La placa no responde", "The board does not answer"),
                            t("¿Es una placa con el firmware FM-1 Rescue? Desconéctala y vuelve a conectarla.",
                              "Is it a board with the FM-1 Rescue firmware? Unplug and re-plug it."), 0);
                } else {
                    silent = 0;
                    report(d, st);
                }
                sleep(500);
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

    private void report(UsbDevice d, RescueStatus s) {
        if (!s.state.equals(lastState)) {
            logLine("FM-1 Rescue: " + s.state + (s.err.equals("none") ? "" : " (" + s.err + " @ " + s.addr + ")"));
            lastState = s.state;
        }
        switch (s.state) {
            case "off":
            case "bad-payload":
                // A plain Transporter, or a damaged payload: reprogram it, once per plug-in.
                if (fwsc != null && loader != null && autoReset.add(d.getDeviceName())) {
                    show(t("Actualizando el RP2040…", "Updating the RP2040…"), "", 0);
                    resetToBootsel(d);
                } else {
                    show(t("El RP2040 no tiene la V15 dentro", "The RP2040 has no V15 inside"),
                            t("Elige el FM-1.fwsc y pulsa \"Volver a grabar el RP2040\".",
                              "Choose the FM-1.fwsc and press \"Reprogram the RP2040\"."), 0);
                }
                break;
            case "wait":
                if (s.foreign) {
                    show(t("Apaga el FM-1", "Switch the FM-1 off"),
                            t("Está encendido con otro firmware. Apágalo, desconecta y vuelve a conectar el RP2040, y enciéndelo.",
                              "It is running another firmware. Switch it off, re-plug the RP2040, switch it on."), 0);
                } else {
                    show(t("Enciende el FM-1 ahora", "Switch the FM-1 on now"),
                            t("Tiene que haber estado apagado. El LED del RP2040 parpadea en amarillo.",
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
                        t("Apaga el FM-1, quita los cables y vuelve a encenderlo: arranca con la V15. "
                          + "Después instala el firmware que quieras de la forma normal.",
                          "Switch the FM-1 off, remove the wires and switch it on: it boots V15. "
                          + "Then install any firmware you like the usual way."), 1000);
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
