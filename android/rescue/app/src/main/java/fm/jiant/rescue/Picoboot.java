// SPDX-License-Identifier: MIT
// PICOBOOT: the RP2040 boot ROM's vendor USB interface (the one picotool uses),
// spoken directly, so the app programs the board with no RPI-RP2 drive and no
// file copy. Commands: 32-byte header on bulk OUT, optional data stage, then a
// zero-length ack in the opposite direction (RP2040 datasheet, 2.8.5).
package fm.jiant.rescue;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Arrays;
import java.util.Map;
import java.util.TreeMap;

public final class Picoboot {
    public static final int VID = 0x2E8A, PID_BOOTSEL = 0x0003;
    static final int MAGIC = 0x431FD10B;
    static final int CMD_EXCLUSIVE_ACCESS = 0x01, CMD_REBOOT = 0x02, CMD_FLASH_ERASE = 0x03,
            CMD_READ = 0x84, CMD_WRITE = 0x05, CMD_EXIT_XIP = 0x06, CMD_ENTER_CMD_XIP = 0x07;
    static final int REQ_INTERFACE_RESET = 0x41;
    static final int TIMEOUT_MS = 5000;

    /** The USB side: bulk endpoints of the PICOBOOT interface and its control requests. */
    public interface Transport {
        /** Returns bytes sent, or < 0 on error. */
        int bulkOut(byte[] data, int len, int timeoutMs);
        /** Returns bytes received, or < 0 on error. */
        int bulkIn(byte[] data, int len, int timeoutMs);
        /** Host-to-device vendor request to the interface (no data). */
        int controlOut(int request, int timeoutMs);
    }

    public interface Progress {
        void step(String what, int done, int total);
    }

    private final Transport t;
    private int token = 1;

    public Picoboot(Transport t) { this.t = t; }

    private void cmd(int id, int cmdSize, int transferLen, byte[] args, byte[] data) throws IOException {
        ByteBuffer b = ByteBuffer.allocate(32).order(ByteOrder.LITTLE_ENDIAN);
        b.putInt(MAGIC).putInt(token++).put((byte) id).put((byte) cmdSize).putShort((short) 0).putInt(transferLen);
        if (args != null) b.put(args);
        byte[] c = b.array();
        if (t.bulkOut(c, 32, TIMEOUT_MS) != 32) throw new IOException("PICOBOOT: command " + hex(id) + " not accepted");
        boolean in = (id & 0x80) != 0;
        if (transferLen > 0) {
            int n = in ? t.bulkIn(data, transferLen, TIMEOUT_MS) : t.bulkOut(data, transferLen, TIMEOUT_MS);
            if (n != transferLen) throw new IOException("PICOBOOT: data stage of " + hex(id) + " failed");
        }
        byte[] zlp = new byte[0];
        int ack = in ? t.bulkOut(zlp, 0, TIMEOUT_MS) : t.bulkIn(new byte[64], 64, TIMEOUT_MS);
        if (ack != 0) throw new IOException("PICOBOOT: command " + hex(id) + " failed (no ack)");
    }

    private static byte[] args(int... v) {
        ByteBuffer b = ByteBuffer.allocate(16).order(ByteOrder.LITTLE_ENDIAN);
        for (int x : v) b.putInt(x);
        return Arrays.copyOf(b.array(), v.length * 4);
    }

    private static String hex(int v) { return String.format("%02X", v); }

    public void reset() { t.controlOut(REQ_INTERFACE_RESET, 1000); }

    public void exclusive() throws IOException { cmd(CMD_EXCLUSIVE_ACCESS, 1, 0, new byte[] {1}, null); }
    public void exitXip() throws IOException { cmd(CMD_EXIT_XIP, 0, 0, null, null); }
    public void enterCmdXip() throws IOException { cmd(CMD_ENTER_CMD_XIP, 0, 0, null, null); }
    public void erase(int addr, int size) throws IOException { cmd(CMD_FLASH_ERASE, 8, 0, args(addr, size), null); }
    public void write(int addr, byte[] d) throws IOException { cmd(CMD_WRITE, 8, d.length, args(addr, d.length), d); }
    public byte[] read(int addr, int size) throws IOException {
        byte[] d = new byte[size];
        cmd(CMD_READ, 8, size, args(addr, size), d);
        return d;
    }
    /** Reboot into the flash firmware (pc = 0, sp = 0). */
    public void reboot(int delayMs) throws IOException { cmd(CMD_REBOOT, 12, 0, args(0, 0, delayMs), null); }

    /** Erase, write and read back every sector of the plan, then boot it. */
    public void program(TreeMap<Integer, byte[]> plan, Progress p) throws IOException {
        int total = plan.size(), i = 0;
        reset();
        exclusive();
        exitXip();
        for (Map.Entry<Integer, byte[]> e : plan.entrySet()) {
            p.step("write", ++i, total);
            erase(e.getKey(), RescueImage.SECTOR);
            write(e.getKey(), e.getValue());
        }
        enterCmdXip();
        i = 0;
        for (Map.Entry<Integer, byte[]> e : plan.entrySet()) {
            p.step("verify", ++i, total);
            if (!Arrays.equals(read(e.getKey(), RescueImage.SECTOR), e.getValue())) {
                throw new IOException(String.format("RP2040 flash verify failed at %08X", e.getKey()));
            }
        }
        p.step("reboot", total, total);
        reboot(300);
    }
}
