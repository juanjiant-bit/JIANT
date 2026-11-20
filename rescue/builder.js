// SPDX-License-Identifier: MIT
// FM-1 Rescue builder: turns the official FM-1.fwsc (V15) and JieLi's
// wl82loader.bin into ONE .uf2 for an RP2040 board. The .uf2 holds the rescue
// firmware (rescue/firmware, a fork of Leo Kuroshita's FM-1 Transporter) and,
// 1 MiB into the board's flash, the payload it restores by itself
// (rescue/firmware/src/payload.h). Runs in the browser and in Node (tests).
//
// The .fwsc parsing is a port of fm1_unbrick.py (Quixotic7/MvaveFM1Unbricker,
// MIT): parse_fwsc, ufw_flash and the V15 region hash.

export const FW_START = 0x4000;
export const FW_LEN = 0x8f000;
export const FW_END = FW_START + FW_LEN;
export const V15_REGION_SHA256 = "6edf3c37fb5bbbc33607c89375ee024d5477c17914d72221c8c68e58a8255686";
export const LOADER_LEN = 24064;
export const LOADER_SHA256 = "d41da6126760c9d66660bcc0cac8d27d221806c5e369a8036921efe68dca5376";
export const LOADER_URL =
  "https://raw.githubusercontent.com/kagaimiq/jl-uboot-tool/adb3f18889e88ac512ce0a3c4d8cc3d3cb30696a/data/loaderblobs/usb/wl82loader.bin";

// payload.h
const PL_FLASH = 0x10100000;    // XIP address of the payload (1 MiB into the flash)
const PL_MAGIC = "FM1-RESCUE-PL-1";
const PL_LOADER_OFF = 0x1000;
const PL_IMAGE_OFF = 0x8000;

const FWSC_BLOCK = 0x2f, FWSC_MARKED_BLOCKS = 20, FWSC_NO_CHAR = 0x7d;

const UF2_MAGIC0 = 0x0a324655, UF2_MAGIC1 = 0x9e5d5157, UF2_MAGIC_END = 0x0ab16f30;
const UF2_FLAG_FAMILY = 0x00002000;
const RP2040_FAMILY = 0xe48bff56;

//--------------------------------------------------------------------+
// checksums
//--------------------------------------------------------------------+

const CRC_T = new Uint32Array(256).map((_, i) => {
  let c = i;
  for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
  return c >>> 0;
});

export function crc32(d, crc = 0) {
  let c = ~crc >>> 0;
  for (let i = 0; i < d.length; i++) c = CRC_T[(c ^ d[i]) & 255] ^ (c >>> 8);
  return ~c >>> 0;
}

function crc16(d, c = 0) {
  for (let i = 0; i < d.length; i++) {
    c ^= d[i] << 8;
    for (let k = 0; k < 8; k++) c = c & 0x8000 ? ((c << 1) ^ 0x1021) & 0xffff : (c << 1) & 0xffff;
  }
  return c;
}

export async function sha256hex(data) {
  const subtle = globalThis.crypto && globalThis.crypto.subtle;
  if (!subtle) throw new Error("this browser has no WebCrypto (open the page over https or as a local file)");
  const h = new Uint8Array(await subtle.digest("SHA-256", data));
  return Array.from(h, b => b.toString(16).padStart(2, "0")).join("");
}

//--------------------------------------------------------------------+
// .fwsc -> FM-1 flash region 0x4000..0x92FFF
//--------------------------------------------------------------------+

function jlEnc(buf) {
  const out = Uint8Array.from(buf);
  let k = 0xffff;
  for (let i = 0; i < out.length; i++) {
    out[i] ^= k & 0xff;
    k = ((k << 1) ^ (k & 0x8000 ? 0x1021 : 0)) & 0xffff;
  }
  return out;
}

const u16 = (p, o) => p[o] | (p[o + 1] << 8);
const u32 = (p, o) => (u16(p, o) | (u16(p, o + 2) << 16)) >>> 0;

export function parseFwsc(data) {
  const stride = FWSC_BLOCK + 1;
  if (data.length < stride * FWSC_MARKED_BLOCKS) throw new Error("not an FM-1 .fwsc package (too short)");
  const head = stride * FWSC_MARKED_BLOCKS;
  const image = new Uint8Array(FWSC_BLOCK * FWSC_MARKED_BLOCKS + data.length - head);
  let identity = "";
  for (let i = 0; i < FWSC_MARKED_BLOCKS; i++) {
    image.set(data.subarray(i * stride, i * stride + FWSC_BLOCK), i * FWSC_BLOCK);
    const m = data[i * stride + FWSC_BLOCK];
    if (m !== FWSC_NO_CHAR) identity += String.fromCharCode((m - i - 1) & 0xff);
  }
  image.set(data.subarray(head), FWSC_BLOCK * FWSC_MARKED_BLOCKS);

  if (image.length < 0x400) throw new Error("not a UFW package");
  const hdr = jlEnc(image.subarray(0, 0x40));
  if (crc16(hdr.subarray(2, 0x40)) !== u16(hdr, 0)) throw new Error("not an FM-1 .fwsc (UFW header CRC)");
  const nent = u16(hdr, 8);
  if (nent === 0 || nent > 11) throw new Error("UFW header lists " + nent + " entries");
  if (crc16(image.subarray(0x40, 0x40 + nent * 0x50)) !== u16(hdr, 2)) throw new Error("UFW entry list CRC: damaged package");
  let fl = null;
  for (let i = 0; i < nent; i++) {
    const e = jlEnc(image.subarray(0x40 + i * 0x50, 0x90 + i * 0x50));
    if (u16(e, 0) === 0) fl = [u32(e, 8), u32(e, 12)];
  }
  if (!fl) throw new Error("no flash.bin in the package");
  const [off, len] = fl;
  if (off + len > image.length) throw new Error("flash.bin runs past the package");
  if (len < FW_END) throw new Error("flash.bin is too short: not a complete FM-1 package");
  const flash = image.subarray(off, off + len);
  return { identity, region: flash.slice(FW_START, FW_END), flashLen: len };
}

// The V15 package, checked by the hash of its firmware region.
export async function checkV15(fwscBytes) {
  const { identity, region } = parseFwsc(fwscBytes);
  const sha = await sha256hex(region);
  if (sha !== V15_REGION_SHA256) {
    throw new Error(
      `this is not the official V15 (identity ${identity || "none"}, region sha256 ${sha.slice(0, 16)}…). ` +
      "Only the official M-VAVE V15 FM-1.fwsc (699,956 bytes) is accepted.");
  }
  return { identity, region, sha };
}

const LOADER_MARK = new TextEncoder().encode("FELUCCA-LOADER-1");
function hasMark(d) {
  outer: for (let i = 0; i + LOADER_MARK.length <= d.length; i++) {
    for (let k = 0; k < LOADER_MARK.length; k++) if (d[i + k] !== LOADER_MARK[k]) continue outer;
    return true;
  }
  return false;
}

// The official V15 (by its region hash), or a JIANT / Felucca-family package made by tools/fm1pkg_make.py:
// identity FM-1_9xx, flash.bin exactly 0x93000, Felucca's own loader in it. JIANT runs under the boot head of
// V15 and of V16, so it is the one for an FM-1 that came with V16. Same rules as RescueImage.checkFirmware.
export async function checkPackage(fwscBytes) {
  const { identity, region, flashLen } = parseFwsc(fwscBytes);
  const sha = await sha256hex(region);
  if (sha === V15_REGION_SHA256) return { identity, region, sha, official: true, label: `${identity || "FM-1_015"} official V15` };
  if (identity === "FM-1_015") throw new Error("it says V15 but its firmware hash is not V15's: damaged or modified");
  if (!/^FM-1_9\d\d$/.test(identity))
    throw new Error(`not the official V15 nor a JIANT package (identity ${identity || "none"}); the official V16 does not fit the rescue`);
  if (flashLen !== FW_END) throw new Error("this package runs past 0x92FFF; the rescue cannot write it");
  if (!hasMark(fwscBytes)) throw new Error("not a package made by JIANT's tools");
  if (region.every(b => b === 0xff || b === 0)) throw new Error("the firmware region of this package is blank");
  return { identity, region, sha, official: false, label: `${identity} JIANT` };
}

export async function checkLoader(bytes) {
  if (bytes.length !== LOADER_LEN) throw new Error(`wl82loader.bin must be ${LOADER_LEN} bytes, not ${bytes.length}`);
  const sha = await sha256hex(bytes);
  if (sha !== LOADER_SHA256) throw new Error("wl82loader.bin has the wrong sha256 " + sha.slice(0, 16) + "…");
  return bytes;
}

//--------------------------------------------------------------------+
// payload + UF2
//--------------------------------------------------------------------+

function hexToBytes(h) {
  return Uint8Array.from(h.match(/../g), x => parseInt(x, 16));
}

export function buildPayload(region, loader, sha, label) {
  const p = new Uint8Array(PL_IMAGE_OFF + FW_LEN).fill(0xff);
  const h = new Uint8Array(256);
  const dv = new DataView(h.buffer);
  for (let i = 0; i < PL_MAGIC.length; i++) h[i] = PL_MAGIC.charCodeAt(i);
  const w = (o, v) => dv.setUint32(o, v >>> 0, true);
  w(16, 1);                 // version
  w(20, 256);               // hdr_size
  w(24, PL_LOADER_OFF);
  w(28, LOADER_LEN);
  w(32, crc32(loader));
  w(36, PL_IMAGE_OFF);
  w(40, FW_START);
  w(44, FW_LEN);
  w(48, crc32(region));
  const lab = label.slice(0, 63);
  for (let i = 0; i < lab.length; i++) h[52 + i] = lab.charCodeAt(i) & 0x7f;
  h.set(hexToBytes(sha), 116);
  w(252, crc32(h.subarray(0, 252)));
  p.set(h, 0);
  p.set(loader, PL_LOADER_OFF);
  p.set(region, PL_IMAGE_OFF);
  return p;
}

export function parseUf2(uf2) {
  if (uf2.length % 512) throw new Error("base .uf2 is not a whole number of blocks");
  const blocks = [];
  for (let o = 0; o < uf2.length; o += 512) {
    const b = uf2.subarray(o, o + 512);
    const dv = new DataView(b.buffer, b.byteOffset, 512);
    if (dv.getUint32(0, true) !== UF2_MAGIC0 || dv.getUint32(4, true) !== UF2_MAGIC1 ||
        dv.getUint32(508, true) !== UF2_MAGIC_END) throw new Error("base .uf2: bad block magic");
    blocks.push({
      flags: dv.getUint32(8, true), addr: dv.getUint32(12, true), size: dv.getUint32(16, true),
      family: dv.getUint32(28, true), data: b.slice(32, 32 + dv.getUint32(16, true)),
    });
  }
  return blocks;
}

function emitUf2(blocks) {
  const out = new Uint8Array(blocks.length * 512);
  blocks.forEach((bl, i) => {
    const b = out.subarray(i * 512, i * 512 + 512);
    const dv = new DataView(out.buffer, i * 512, 512);
    dv.setUint32(0, UF2_MAGIC0, true);
    dv.setUint32(4, UF2_MAGIC1, true);
    dv.setUint32(8, bl.flags, true);
    dv.setUint32(12, bl.addr, true);
    dv.setUint32(16, bl.data.length, true);
    dv.setUint32(20, i, true);
    dv.setUint32(24, blocks.length, true);
    dv.setUint32(28, bl.family, true);
    b.set(bl.data, 32);
    dv.setUint32(508, UF2_MAGIC_END, true);
  });
  return out;
}

// base firmware .uf2 + payload -> one .uf2 the RP2040 boot ROM takes in one copy.
export function mergeUf2(baseUf2, payload) {
  const base = parseUf2(baseUf2);
  let end = 0;
  for (const b of base) {
    if (b.family !== RP2040_FAMILY || !(b.flags & UF2_FLAG_FAMILY)) throw new Error("base .uf2 is not an RP2040 image");
    if (b.addr < 0x10000000) throw new Error("base .uf2 writes outside the flash");
    end = Math.max(end, b.addr + b.size);
  }
  if (end > PL_FLASH) throw new Error("base firmware overlaps the payload area");
  const blocks = base.map(b => ({ ...b }));
  // Header page, loader, image: pages of 256 bytes. The 0xFF gap between the
  // loader and the image is not sent (its sectors are erased by the boot ROM
  // or never read).
  const ranges = [[0, 256], [PL_LOADER_OFF, PL_LOADER_OFF + LOADER_LEN],
                  [PL_IMAGE_OFF, PL_IMAGE_OFF + FW_LEN]];
  for (const [lo, hi] of ranges) {
    for (let o = lo; o < hi; o += 256) {
      const data = new Uint8Array(256).fill(0xff);
      data.set(payload.subarray(o, Math.min(o + 256, hi)));
      blocks.push({ flags: UF2_FLAG_FAMILY, addr: PL_FLASH + o, size: 256, family: RP2040_FAMILY, data });
    }
  }
  return emitUf2(blocks);
}

export async function buildRescueUf2({ baseUf2, fwsc, loader, label }) {
  const v = await checkPackage(fwsc);
  await checkLoader(loader);
  const payload = buildPayload(v.region, loader, v.sha, label || v.label);
  return { uf2: mergeUf2(baseUf2, payload), identity: v.identity, official: v.official };
}
