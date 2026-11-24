// SPDX-License-Identifier: MIT
// Diagnosis on real FM-1 Transporter logs: android/rescue/test/run_diag.sh LOG...
import fm.jiant.rescue.Diagnosis;
import java.nio.file.Files;
import java.nio.file.Path;

public class DiagTest {
    public static void main(String[] a) throws Exception {
        for (String f : a) {
            Diagnosis d = new Diagnosis();
            int n = 0;
            for (String line : Files.readAllLines(Path.of(f))) {
                int i = line.indexOf("console: ");
                d.feed(i >= 0 ? line.substring(i + 9) : line);
                n++;
            }
            String[] s = d.summary(true);
            System.out.println(Path.of(f).getFileName() + " (" + n + " lines)\n  -> " + s[0] + "\n     " + s[1]);
        }
        // synthetic cases
        String[][] cases = {
            {"[ 1 us] KEY: sending 0x16EF", "[ 2 us] KEY: 4070 packets"},
            {"[ 1 us] KEY: sending 0x16EF", "[ 2 us] KEY: 300000 packets"},
            {"[ 1 us] KEY: sending", "D- pulled up instead of D+ - are D+/D- wired swapped?"},
            {"[ 1 us] ACK (data line held low) after 10 packets", "[ 2 us] HANDOFF: x", "[ 9 us] HOST: no device mounted yet (2.0 s after host start)"},
            {"[1 us] RESCUE: ERROR at 000000: this is not an FM-1 (chip key / flash ID)", "RESCUE: chip key 1234, type 3, flash id EF4014"},
            {"RESCUE: DONE in 80.1 s."},
        };
        for (String[] c : cases) {
            Diagnosis d = new Diagnosis();
            for (String l : c) d.feed(l);
            String[] s = d.summary(true);
            System.out.println("synthetic: " + c[c.length - 1].substring(0, Math.min(40, c[c.length - 1].length())) + "\n  -> " + s[0] + "\n     " + s[1]);
        }
    }
}
