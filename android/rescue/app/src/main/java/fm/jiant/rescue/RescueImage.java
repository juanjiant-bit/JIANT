// SPDX-License-Identifier: MIT
// FM-1 Rescue: what gets programmed into the RP2040. The same steps as
// rescue/builder.js (the web page), in Java: the official V15 region out of
// FM-1.fwsc, JieLi's loader, the payload (rescue/firmware/src/payload.h) and
// the base rescue firmware (a .uf2 in the app's assets), merged into the
// 4 KiB flash sectors the app writes over PICOBOOT. No Android classes here,
// so android/rescue/test runs it on a computer.
package fm.jiant.rescue;

import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Arrays;
import java.util.Map;
import java.util.TreeMap;
import java.util.zip.CRC32;

public final class RescueImage {
    public static final int FW_START = 0x4000;
    public static final int FW_LEN = 0x8F000;
    public static final int FW_END = FW_START + FW_LEN;
    public static final String V15_REGION_SHA256 =
            "6edf3c37fb5bbbc33607c89375ee024d5477c17914d72221c8c68e58a8255686";
    public static final int LOADER_LEN = 24064;
    public static final String LOADER_SHA256 =
            "d41da6126760c9d66660bcc0cac8d27d221806c5e369a8036921efe68dca5376";
    /** The official M-VAVE V15 FM-1.fwsc (file sha256 db1642b2..., the genuine one), as kept in
     *  Quixotic7/FM1Backups at a pinned commit; checkV15 accepts it only by its region hash. */
    public static final String V15_URL = "https://raw.githubusercontent.com/Quixotic7/FM1Backups/"
            + "8cbc3b066ed15ec6a0d5a990efb78b56f6c17183/V15-FM-1.fwsc";
    public static final String LOADER_URL =
            "https://raw.githubusercontent.com/kagaimiq/jl-uboot-tool/"
            + "adb3f18889e88ac512ce0a3c4d8cc3d3cb30696a/data/loaderblobs/usb/wl82loader.bin";

    static final int FLASH_BASE = 0x10000000;
    static final int PL_FLASH = 0x10100000;
    static final int PL_LOADER_OFF = 0x1000;
    static final int PL_IMAGE_OFF = 0x8000;
    static final int SECTOR = 4096;

    private static final int FWSC_BLOCK = 0x2F, FWSC_MARKED_BLOCKS = 20, FWSC_NO_CHAR = 0x7D;
    private static final int UF2_MAGIC0 = 0x0A324655, UF2_MAGIC1 = 0x9E5D5157,
            UF2_MAGIC_END = 0x0AB16F30, UF2_FLAG_FAMILY = 0x2000, RP2040_FAMILY = 0xE48BFF56;

    public static final class Refused extends Exception {
        public Refused(String msg) { super(msg); }
    }

    /** The V15 region and the package identity. */
    public static final class V15 {
        public final String identity;
        public final byte[] region;
        public final String sha256;
        V15(String identity, byte[] region, String sha256) {
            this.identity = identity; this.region = region; this.sha256 = sha256;
        }
    }

    private RescueImage() {}

    // ---- checksums ----

    public static String sha256(byte[] d) {
        try {
            byte[] h = MessageDigest.getInstance("SHA-256").digest(d);
            StringBuilder s = new StringBuilder();
            for (byte b : h) s.append(String.format("%02x", b & 0xFF));
            return s.toString();
        } catch (Exception e) {
            throw new IllegalStateException(e);
        }
    }

    static int crc32(byte[] d, int off, int len) {
        CRC32 c = new CRC32();
        c.update(d, off, len);
        return (int) c.getValue();
    }

    private static int crc16(byte[] d, int off, int len) {
        int c = 0;
        for (int i = off; i < off + len; i++) {
            c ^= (d[i] & 0xFF) << 8;
            for (int k = 0; k < 8; k++) c = (c & 0x8000) != 0 ? ((c << 1) ^ 0x1021) & 0xFFFF : (c << 1) & 0xFFFF;
        }
        return c;
    }

    private static byte[] jlEnc(byte[] src, int off, int len) {
        byte[] out = Arrays.copyOfRange(src, off, off + len);
        int k = 0xFFFF;
        for (int i = 0; i < out.length; i++) {
            out[i] ^= (byte) k;
            k = ((k << 1) ^ ((k & 0x8000) != 0 ? 0x1021 : 0)) & 0xFFFF;
        }
        return out;
    }

    private static int u16(byte[] p, int o) { return (p[o] & 0xFF) | (p[o + 1] & 0xFF) << 8; }
    private static int u32(byte[] p, int o) { return u16(p, o) | u16(p, o + 2) << 16; }

    // ---- .fwsc ----

    public static V15 checkV15(byte[] data) throws Refused {
        int stride = FWSC_BLOCK + 1, head = stride * FWSC_MARKED_BLOCKS;
        if (data.length < head) throw new Refused("not an FM-1 .fwsc package (too short)");
        ByteArrayOutputStream img = new ByteArrayOutputStream(data.length);
        StringBuilder id = new StringBuilder();
        for (int i = 0; i < FWSC_MARKED_BLOCKS; i++) {
            img.write(data, i * stride, FWSC_BLOCK);
            int m = data[i * stride + FWSC_BLOCK] & 0xFF;
            if (m != FWSC_NO_CHAR) id.append((char) ((m - i - 1) & 0xFF));
        }
        img.write(data, head, data.length - head);
        byte[] image = img.toByteArray();

        if (image.length < 0x400) throw new Refused("not a UFW package");
        byte[] hdr = jlEnc(image, 0, 0x40);
        if (crc16(hdr, 2, 0x3E) != u16(hdr, 0)) throw new Refused("not an FM-1 .fwsc (UFW header CRC)");
        int nent = u16(hdr, 8);
        if (nent == 0 || nent > 11) throw new Refused("UFW header lists " + nent + " entries");
        if (crc16(image, 0x40, nent * 0x50) != u16(hdr, 2)) throw new Refused("damaged package (UFW entry CRC)");
        long off = -1, len = 0;
        for (int i = 0; i < nent; i++) {
            byte[] e = jlEnc(image, 0x40 + i * 0x50, 0x50);
            if (u16(e, 0) == 0) { off = u32(e, 8) & 0xFFFFFFFFL; len = u32(e, 12) & 0xFFFFFFFFL; }
        }
        if (off < 0) throw new Refused("no flash.bin in the package");
        if (off + len > image.length) throw new Refused("flash.bin runs past the package");
        if (len < FW_END) throw new Refused("not a complete FM-1 package");
        byte[] region = Arrays.copyOfRange(image, (int) off + FW_START, (int) off + FW_END);
        String sha = sha256(region);
        if (!sha.equals(V15_REGION_SHA256)) {
            throw new Refused("this is not the official V15 (" + (id.length() > 0 ? id : "no identity")
                    + "). Only M-VAVE's V15 FM-1.fwsc (699,956 bytes) is accepted.");
        }
        return new V15(id.toString(), region, sha);
    }

    public static byte[] checkLoader(byte[] b) throws Refused {
        if (b.length != LOADER_LEN) throw new Refused("wl82loader.bin must be " + LOADER_LEN + " bytes");
        if (!sha256(b).equals(LOADER_SHA256)) throw new Refused("wl82loader.bin has the wrong sha256");
        return b;
    }

    // ---- payload ----

    public static byte[] buildPayload(V15 v, byte[] loader) {
        byte[] p = new byte[PL_IMAGE_OFF + FW_LEN];
        Arrays.fill(p, (byte) 0xFF);
        ByteBuffer h = ByteBuffer.wrap(p, 0, 256).order(ByteOrder.LITTLE_ENDIAN);
        Arrays.fill(p, 0, 256, (byte) 0);
        byte[] magic = "FM1-RESCUE-PL-1".getBytes(StandardCharsets.US_ASCII);
        System.arraycopy(magic, 0, p, 0, magic.length);
        h.putInt(16, 1).putInt(20, 256).putInt(24, PL_LOADER_OFF).putInt(28, LOADER_LEN)
         .putInt(32, crc32(loader, 0, loader.length)).putInt(36, PL_IMAGE_OFF).putInt(40, FW_START)
         .putInt(44, FW_LEN).putInt(48, crc32(v.region, 0, v.region.length));
        byte[] label = ((v.identity.isEmpty() ? "FM-1_015" : v.identity) + " official V15")
                .getBytes(StandardCharsets.US_ASCII);
        System.arraycopy(label, 0, p, 52, Math.min(label.length, 63));
        for (int i = 0; i < 32; i++) p[116 + i] = (byte) Integer.parseInt(v.sha256.substring(2 * i, 2 * i + 2), 16);
        h.putInt(252, crc32(p, 0, 252));
        System.arraycopy(loader, 0, p, PL_LOADER_OFF, LOADER_LEN);
        System.arraycopy(v.region, 0, p, PL_IMAGE_OFF, FW_LEN);
        return p;
    }

    // ---- UF2 / flash sectors ----

    /** Flash address -> 4 KiB sector content, for every sector the UF2 touches (rest 0xFF). */
    public static TreeMap<Integer, byte[]> sectorsOfUf2(byte[] uf2) throws Refused {
        if (uf2.length == 0 || uf2.length % 512 != 0) throw new Refused("base firmware is not a UF2");
        TreeMap<Integer, byte[]> out = new TreeMap<>();
        ByteBuffer b = ByteBuffer.wrap(uf2).order(ByteOrder.LITTLE_ENDIAN);
        for (int o = 0; o < uf2.length; o += 512) {
            if (b.getInt(o) != UF2_MAGIC0 || b.getInt(o + 4) != UF2_MAGIC1 || b.getInt(o + 508) != UF2_MAGIC_END)
                throw new Refused("base firmware: bad UF2 block");
            int flags = b.getInt(o + 8), addr = b.getInt(o + 12), size = b.getInt(o + 16), fam = b.getInt(o + 28);
            if ((flags & UF2_FLAG_FAMILY) == 0 || fam != RP2040_FAMILY) throw new Refused("base firmware is not for the RP2040");
            if (size <= 0 || size > 476) throw new Refused("base firmware: bad UF2 block size");
            put(out, addr, uf2, o + 32, size);
        }
        return out;
    }

    private static void put(TreeMap<Integer, byte[]> map, int addr, byte[] src, int off, int len) throws Refused {
        long a = addr & 0xFFFFFFFFL;
        if (a < FLASH_BASE || a + len > FLASH_BASE + 0x200000L) throw new Refused("write outside the 2 MiB flash");
        for (int i = 0; i < len; ) {
            int sec = (int) ((a + i) & ~(SECTOR - 1L));
            int in = (int) ((a + i) - (sec & 0xFFFFFFFFL));
            int n = Math.min(len - i, SECTOR - in);
            byte[] s = map.get(sec);
            if (s == null) { s = new byte[SECTOR]; Arrays.fill(s, (byte) 0xFF); map.put(sec, s); }
            System.arraycopy(src, off + i, s, in, n);
            i += n;
        }
    }

    /** Base firmware + payload, as the flash sectors to write. */
    public static TreeMap<Integer, byte[]> flashPlan(byte[] baseUf2, byte[] payload) throws Refused {
        TreeMap<Integer, byte[]> plan = sectorsOfUf2(baseUf2);
        for (Map.Entry<Integer, byte[]> e : plan.entrySet()) {
            if ((e.getKey() & 0xFFFFFFFFL) >= (PL_FLASH & 0xFFFFFFFFL)) throw new Refused("base firmware overlaps the payload");
        }
        int[][] ranges = {{0, 256}, {PL_LOADER_OFF, PL_LOADER_OFF + LOADER_LEN}, {PL_IMAGE_OFF, PL_IMAGE_OFF + FW_LEN}};
        for (int[] r : ranges) put(plan, PL_FLASH + r[0], payload, r[0], r[1] - r[0]);
        return plan;
    }

    public static TreeMap<Integer, byte[]> build(byte[] baseUf2, byte[] fwsc, byte[] loader) throws Refused {
        V15 v = checkV15(fwsc);
        return flashPlan(baseUf2, buildPayload(v, checkLoader(loader)));
    }
}
