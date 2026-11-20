// SPDX-License-Identifier: MIT
// Computer-side test of the app's core (no Android, no phone):
//   android/rescue/test/run.sh FM-1.fwsc V16.fwsc wl82loader.bin WEB.uf2 BASE_ZERO.uf2
// WEB.uf2 is what rescue/fm1-rescue.html makes for the RP2040-Zero; the app
// must leave exactly that in the board's flash. The RP2040 is a mock that
// speaks PICOBOOT with real flash rules (erase to 0xFF, program only clears
// bits, flash reads only in command XIP mode, writes only after EXIT_XIP).
import fm.jiant.rescue.Picoboot;
import fm.jiant.rescue.RescueImage;
import fm.jiant.rescue.RescueStatus;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.TreeMap;

public class RescueTest {
    static int failures;
    static void check(boolean ok, String what) {
        if (!ok) { failures++; System.out.println("  FAIL: " + what); }
    }

    static class MockRp2040 implements Picoboot.Transport {
        final byte[] flash = new byte[2 * 1024 * 1024];
        boolean exclusive, xipOff, rebooted;
        int erases, writes;
        // command in progress
        ByteBuffer cmd; int stage; // 0 = expect command, 1 = data, 2 = ack
        byte[] readBack;
        MockRp2040() { Arrays.fill(flash, (byte) 0x5A); }

        int addr() { return cmd.getInt(16) - 0x10000000; }
        int size() { return cmd.getInt(20); }

        public int bulkOut(byte[] d, int len, int to) {
            if (stage == 0) {
                if (len != 32) return -1;
                cmd = ByteBuffer.wrap(Arrays.copyOf(d, 32)).order(ByteOrder.LITTLE_ENDIAN);
                if (cmd.getInt(0) != 0x431FD10B) return -1;
                int id = cmd.get(8) & 0xFF, tl = cmd.getInt(12);
                switch (id) {
                    case 0x01: exclusive = cmd.get(16) == 1; break;
                    case 0x06: xipOff = true; break;
                    case 0x07: xipOff = false; break;
                    case 0x02: rebooted = true; break;
                    case 0x03:
                        if (!exclusive || !xipOff || addr() % 4096 != 0 || size() % 4096 != 0) return -1;
                        Arrays.fill(flash, addr(), addr() + size(), (byte) 0xFF);
                        erases++;
                        break;
                    case 0x84:
                        if (xipOff) return -1;   // flash not readable with XIP off
                        readBack = Arrays.copyOfRange(flash, addr(), addr() + size());
                        break;
                    case 0x05:
                        if (!exclusive || !xipOff || addr() % 256 != 0 || size() % 256 != 0 || tl != size()) return -1;
                        break;
                    default: return -1;
                }
                stage = tl > 0 ? 1 : 2;
                return 32;
            }
            int id = cmd.get(8) & 0xFF;
            if (stage == 1 && id == 0x05) {
                if (len != size()) return -1;
                for (int i = 0; i < len; i++) flash[addr() + i] &= d[i];   // program only clears bits
                writes++;
                stage = 2;
                return len;
            }
            if (stage == 2 && (id & 0x80) != 0 && len == 0) { stage = 0; return 0; }
            return -1;
        }

        public int bulkIn(byte[] d, int len, int to) {
            int id = cmd == null ? 0 : cmd.get(8) & 0xFF;
            if (stage == 1 && id == 0x84) {
                System.arraycopy(readBack, 0, d, 0, size());
                stage = 2;
                return size();
            }
            if (stage == 2 && (id & 0x80) == 0) { stage = 0; return 0; }
            return -1;
        }

        public int controlOut(int req, int to) { stage = 0; return 0; }
    }

    static byte[] flashOfUf2(byte[] uf2) {
        byte[] f = new byte[2 * 1024 * 1024];
        Arrays.fill(f, (byte) 0x5A);   // untouched sectors keep old contents
        ByteBuffer b = ByteBuffer.wrap(uf2).order(ByteOrder.LITTLE_ENDIAN);
        // The boot ROM erases each 4 KiB sector it writes, then programs the pages.
        java.util.Set<Integer> erased = new java.util.HashSet<>();
        for (int o = 0; o < uf2.length; o += 512) {
            int a = b.getInt(o + 12) - 0x10000000, n = b.getInt(o + 16);
            if (erased.add(a & ~4095)) Arrays.fill(f, a & ~4095, (a & ~4095) + 4096, (byte) 0xFF);
            System.arraycopy(uf2, o + 32, f, a, n);
        }
        return f;
    }

    public static void main(String[] a) throws Exception {
        byte[] v15 = Files.readAllBytes(Path.of(a[0])), v16 = Files.readAllBytes(Path.of(a[1]));
        byte[] loader = Files.readAllBytes(Path.of(a[2])), web = Files.readAllBytes(Path.of(a[3]));
        byte[] base = Files.readAllBytes(Path.of(a[4]));

        System.out.println("1. V15 accepted, V16 and a damaged package refused, loader checked");
        RescueImage.V15 v = RescueImage.checkV15(v15);
        check(v.identity.equals("FM-1_015"), "identity " + v.identity);
        try { RescueImage.checkV15(v16); check(false, "V16 accepted"); } catch (RescueImage.Refused e) { }
        byte[] bad = v15.clone(); bad[40000] ^= 1;
        try { RescueImage.checkV15(bad); check(false, "damaged V15 accepted"); } catch (RescueImage.Refused e) { }
        byte[] badLoader = loader.clone(); badLoader[5] ^= 1;
        try { RescueImage.checkLoader(badLoader); check(false, "bad loader accepted"); } catch (RescueImage.Refused e) { }

        System.out.println("2. PICOBOOT programming leaves exactly the web page's .uf2 in the flash");
        TreeMap<Integer, byte[]> plan = RescueImage.build(base, v15, loader);
        MockRp2040 rp = new MockRp2040();
        int[] last = {0};
        new Picoboot(rp).program(plan, (w, d, t) -> last[0] = d);
        check(rp.rebooted, "not rebooted");
        check(rp.erases == plan.size() && rp.writes == plan.size(), "erases " + rp.erases + " writes " + rp.writes);
        check(Arrays.equals(rp.flash, flashOfUf2(web)), "flash differs from the web .uf2");

        System.out.println("3. a flash that does not take the write is reported");
        MockRp2040 stuck = new MockRp2040() {
            @Override public int bulkOut(byte[] d, int len, int to) {
                int r = super.bulkOut(d, len, to);
                if (stage == 2 && (cmd.get(8) & 0xFF) == 0x05 && addr() == 0x108000) flash[addr()] = 0;
                return r;
            }
        };
        try { new Picoboot(stuck).program(plan, (w, d, t) -> {}); check(false, "no error"); }
        catch (IOException e) { check(e.getMessage().contains("10108000"), e.getMessage()); }

        System.out.println("4. status lines");
        RescueStatus s = RescueStatus.parse("OK state=run phase=write sector=72/143 written=3 same=69 err=none addr=000000");
        check(s != null && s.state.equals("run") && s.sector == 72 && s.written == 3 && s.permille() > 300 && s.permille() < 340,
              "run line");
        s = RescueStatus.parse("OK state=wait foreign=1 phase=idle sector=0/143 written=0 same=0 err=none addr=000000");
        check(s != null && s.foreign && s.permille() == 0, "foreign line");
        s = RescueStatus.parse("OK state=error phase=write sector=9/143 written=8 same=0 err=write addr=00C000");
        check(s != null && s.err.equals("write") && s.addr.equals("00C000"), "error line");
        check(RescueStatus.parse("PONG") == null, "PONG parsed");

        System.out.println(failures == 0 ? "\nall app core tests passed" : "\n" + failures + " FAILURE(S)");
        System.exit(failures == 0 ? 0 : 1);
    }
}
