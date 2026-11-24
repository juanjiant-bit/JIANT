// SPDX-License-Identifier: MIT
// Reads the rescue firmware's console (CDC 0: the FM-1 Transporter's log, src/recovery.c, pio_host.c,
// autorescue.c) and says in plain words how far the FM-1 got and what to try next. No Android classes:
// android/rescue/test runs it on real Transporter logs.
package fm.jiant.rescue;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class Diagnosis {
    private static final Pattern PACKETS = Pattern.compile("KEY: (\\d+) packets");
    private static final Pattern ACK = Pattern.compile("(ACK \\(data line held low\\)|D\\+ high in gap).* after (\\d+) packets");
    private static final Pattern VIDPID = Pattern.compile("VID=([0-9A-Fa-f]{4}) PID=([0-9A-Fa-f]{4})");
    private static final Pattern CHIP = Pattern.compile("chip key ([0-9A-Fa-f]{4}), type \\d+, flash id ([0-9A-Fa-f]+)");

    public boolean keying, ack, falseAck, swapped, handoff, mounted, uboot, ready, inquiryFailed,
            noMountYet, reattach, attachedAtBoot, stockV15, otherDevice, done, error, chipBad;
    public long packets, ackAfter;
    public int falseAcks, reattaches;
    public String vidPid = "", chip = "", errorLine = "";

    /** Feeds one console line (with or without its "[  123 us] " prefix). */
    public void feed(String line) {
        if (line == null) return;
        Matcher m;
        if (line.contains("KEY: sending")) keying = true;
        if ((m = PACKETS.matcher(line)).find()) { keying = true; packets = Long.parseLong(m.group(1)); }
        if ((m = ACK.matcher(line)).find()) { ack = true; ackAfter = Long.parseLong(m.group(2)); }
        if (line.contains("false ACK")) { falseAck = true; falseAcks++; }
        if (line.contains("D- pulled up instead of D+")) swapped = true;
        if (line.contains("HANDOFF:")) handoff = true;
        if (line.contains("D+ already pulled up")) attachedAtBoot = true;
        if (line.contains("HOST: device mounted")) mounted = true;
        if ((m = VIDPID.matcher(line)).find()) {
            vidPid = (m.group(1) + ":" + m.group(2)).toUpperCase();
            if (vidPid.equals("4C4A:8057")) uboot = true;
            else if (vidPid.equals("4C4A:C755")) stockV15 = true;
            else otherDevice = true;
        }
        if (line.contains("UBOOT READY")) { ready = true; uboot = true; }
        if (line.contains("INQUIRY never accepted")) inquiryFailed = true;
        if (line.contains("HOST: no device mounted yet")) noMountYet = true;
        if (line.contains("rebooting the transporter to pulse and re-host")) { reattach = true; reattaches++; }
        if ((m = CHIP.matcher(line)).find()) chip = m.group(1).toUpperCase() + " / " + m.group(2).toUpperCase();
        if (line.contains("RESCUE: DONE")) done = true;
        if (line.contains("RESCUE: ERROR")) {
            error = true;
            errorLine = line.replaceFirst("^\\[[^]]*\\]\\s*", "");
            if (line.contains("not an FM-1")) chipBad = true;
        }
    }

    /** Lines that only make the on-screen log longer (TinyUSB host debug). */
    public static boolean noise(String line) {
        return line.matches("^\\[\\d+\\] Aborted transfer.*") || line.matches("^\\s*(Open|Allocated|Bind) .*")
                || line.matches("^\\[\\d+:\\d*\\] (Open EP|Get Descriptor|Set Address|Control data|Set Configuration).*")
                || line.trim().isEmpty();
    }

    /** {headline, advice} in Spanish or English. */
    public String[] summary(boolean es) {
        if (done) return pair(es, "El FM-1 quedó reparado", "The FM-1 is repaired",
                "Todo el proceso terminó bien.", "The whole process finished fine.");
        if (swapped) return pair(es, "Cables D+ y D− invertidos", "D+ and D− wires swapped",
                "El RP2040 vio que el FM-1 respondía por el hilo equivocado. Intercambia los hilos de GP0 y GP1 "
                + "(verde/celeste y blanco), apaga el FM-1, reconecta el RP2040 y vuelve a encenderlo.",
                "The RP2040 saw the FM-1 answer on the wrong wire. Swap the GP0 and GP1 wires (green and white), "
                + "switch the FM-1 off, re-plug the RP2040 and switch it on again.");
        if (chipBad) return pair(es, "El chip no es el de un FM-1", "The chip is not an FM-1's",
                "Respondió algo, pero con otra clave o memoria (" + chip + "). No se escribió nada.",
                "Something answered, but with another key or flash (" + chip + "). Nothing was written.");
        if (error) return pair(es, "Error durante la reparación", "Error during the repair",
                "El FM-1 entró en modo de arranque, así que la conexión funciona. Mensaje: " + errorLine
                + ". Apaga el FM-1, reconecta el RP2040 y repite.",
                "The FM-1 entered boot mode, so the link works. Message: " + errorLine
                + ". Switch the FM-1 off, re-plug the RP2040 and repeat.");
        if (ready) return pair(es, "El FM-1 está en modo de arranque ✓", "The FM-1 is in boot mode ✓",
                "La comunicación funciona" + (chip.isEmpty() ? "." : " (chip " + chip + ")."),
                "The link works" + (chip.isEmpty() ? "." : " (chip " + chip + ")."));
        if (mounted && inquiryFailed) return pair(es, "Se conectó pero no acepta órdenes", "Attached but takes no commands",
                "El FM-1 apareció por USB (" + vidPid + ") pero no contestó a las órdenes. Suele ser un contacto "
                + "flojo o cables largos: acorta los hilos, revisa las soldaduras y repite.",
                "The FM-1 showed up on USB (" + vidPid + ") but did not answer commands. Usually a loose contact "
                + "or long wires: shorten them, check the solder joints and repeat.");
        if (stockV15) return pair(es, "El FM-1 arrancó con la V15 de fábrica", "The FM-1 booted stock V15",
                "No está brickeado: arrancó su firmware. La app le envía la orden para entrar en modo de arranque.",
                "It is not bricked: it booted its firmware. The app sends it the command to enter boot mode.");
        if (otherDevice) return pair(es, "El FM-1 arrancó otro firmware", "The FM-1 booted another firmware",
                "Apareció como " + vidPid + ". Apágalo, desconecta y reconecta el RP2040, y enciéndelo de nuevo.",
                "It showed up as " + vidPid + ". Switch it off, re-plug the RP2040 and switch it on again.");
        if (ack && !mounted && (noMountYet || reattach)) return pair(es, "Respondió, pero no se conecta por USB", "It answered but does not attach",
                "El FM-1 contestó a la señal de arranque (tras " + ackAfter + " envíos) pero después no apareció por "
                + "USB" + (reattaches > 0 ? " (" + reattaches + " reintentos)" : "") + ". Los cables y el FM-1 "
                + "responden: suele ser ruido o un contacto flojo en D+/D−. Acorta los hilos, que no estén cerca del "
                + "rojo, y repite apagando y encendiendo el FM-1.",
                "The FM-1 answered the boot signal (after " + ackAfter + " packets) but then did not show up on "
                + "USB" + (reattaches > 0 ? " (" + reattaches + " retries)" : "") + ". The wires and the FM-1 "
                + "respond: usually noise or a loose contact on D+/D−. Shorten the wires and repeat, switching the "
                + "FM-1 off and on.");
        if (ack) return pair(es, "¡El FM-1 respondió! ✓", "The FM-1 answered! ✓",
                "Contestó a la señal de arranque tras " + ackAfter + " envíos" + (mounted ? " y se conectó por USB." : "; conectando…"),
                "It answered the boot signal after " + ackAfter + " packets" + (mounted ? " and attached on USB." : "; attaching…"));
        if (attachedAtBoot) return pair(es, "El FM-1 ya estaba encendido", "The FM-1 was already on",
                "Para un FM-1 brickeado: apágalo, desconecta y reconecta el RP2040, y enciende el FM-1 cuando la app lo diga.",
                "For a bricked FM-1: switch it off, re-plug the RP2040 and switch the FM-1 on when the app says so.");
        if (keying) {
            String n = String.format("%,d", packets);
            if (packets < 60000) return pair(es, "Enviando la señal de arranque…", "Sending the boot signal…",
                    "Envíos: " + n + (falseAcks > 0 ? " (" + falseAcks + " falsas respuestas)" : "") + ". Enciende el FM-1 "
                    + "ahora. Puede tardar: en un FM-1 real hicieron falta más de 3 minutos.",
                    "Packets: " + n + (falseAcks > 0 ? " (" + falseAcks + " false answers)" : "") + ". Switch the FM-1 "
                    + "on now. It can take a while: on a real FM-1 it took over 3 minutes.");
            return pair(es, "El FM-1 todavía no responde", "The FM-1 does not answer yet",
                    "Envíos: " + n + ". Prueba en este orden: 1) apaga y enciende el FM-1 (deja la app así); 2) carga "
                    + "el FM-1 una hora con un cable normal y repite; 3) intercambia los hilos de GP0 y GP1; 4) revisa con "
                    + "un multímetro que cada hilo llegue a su contacto del USB-C (D+ = A6/B6, D− = A7/B7, GND = carcasa).",
                    "Packets: " + n + ". Try in this order: 1) switch the FM-1 off and on (leave the app as it is); 2) "
                    + "charge the FM-1 for an hour with a normal cable and repeat; 3) swap the GP0 and GP1 wires; 4) "
                    + "check with a multimeter that each wire reaches its USB-C contact (D+ = A6/B6, D− = A7/B7, GND = shell).");
        }
        return pair(es, "Esperando el registro del RP2040", "Waiting for the RP2040's log", "", "");
    }

    private static String[] pair(boolean es, String hEs, String hEn, String aEs, String aEn) {
        return es ? new String[] {hEs, aEs} : new String[] {hEn, aEn};
    }
}
