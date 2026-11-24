// SPDX-License-Identifier: MIT
// The rescue firmware's answer to `rescue` on its data port (ar_status_line in
// rescue/firmware/src/autorescue.c), e.g.
//   OK state=run phase=write sector=17/143 written=5 same=12 err=none addr=000000
package fm.jiant.rescue;

import java.util.HashMap;
import java.util.Map;

public final class RescueStatus {
    public final String state, phase, err, addr;
    public final int sector, sectors, written, same;
    public final boolean foreign;

    private RescueStatus(Map<String, String> m) {
        state = m.getOrDefault("state", "?");
        phase = m.getOrDefault("phase", "idle");
        err = m.getOrDefault("err", "none");
        addr = m.getOrDefault("addr", "");
        foreign = "1".equals(m.get("foreign"));
        written = num(m.get("written"));
        same = num(m.get("same"));
        String s = m.getOrDefault("sector", "0/143");
        int slash = s.indexOf('/');
        sector = slash > 0 ? num(s.substring(0, slash)) : 0;
        sectors = slash > 0 ? Math.max(1, num(s.substring(slash + 1))) : 143;
    }

    private static int num(String s) {
        try { return s == null ? 0 : Integer.parseInt(s.trim()); } catch (NumberFormatException e) { return 0; }
    }

    /** null if the line is not a rescue status line. */
    public static RescueStatus parse(String line) {
        if (line == null) return null;
        line = line.trim();
        if (!line.startsWith("OK state=")) return null;
        Map<String, String> m = new HashMap<>();
        for (String tok : line.substring(3).split("\\s+")) {
            int eq = tok.indexOf('=');
            if (eq > 0) m.put(tok.substring(0, eq), tok.substring(eq + 1));
        }
        return new RescueStatus(m);
    }

    /** 0..1000 over the whole restore: check, write pass, verify pass. */
    public int permille() {
        if ("done".equals(state)) return 1000;
        if (!"run".equals(state)) return 0;
        double f = (double) sector / sectors;
        switch (phase) {
            case "write": return (int) (20 + 600 * f);
            case "verify": return (int) (620 + 380 * f);
            default: return 10;
        }
    }
}
