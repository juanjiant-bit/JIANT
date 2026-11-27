// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments (fm1pkg.js, Felucca)
// Copyright (C) 2026 genkigenki (the SMK-37 range's stock packages: the 36-block layout, identifyStockPackage;
//   https://genkigenki.github.io/smk37/, GPL-3.0 installer code)
// Copyright (C) 2026 Juan Jiant (JIANT FM: Studio's SMK-37 installer)
//
// M-VAVE's own SMK-37 packages (V015 / V016), for the way back to the official firmware. The user downloads them from
// M-VAVE: nothing of M-VAVE's is shipped here. They interleave 36 marker blocks (FM-1 and JIANT packages: 20), so
// the identity and the image the loader writes are read with the detected block count; the files' CRCs must agree.

const BLOCKS = 20, BLK = 0x30, KEEP = 0x2F, MAX_BLOCKS = 64;

// the published releases (informational: a package is accepted by its structure, so a later M-VAVE release works)
export const STOCK_SMK = [
  { name: "SMK-37 Pro", version: "016", product: "SMK-37 Pro_016", size: 705252, sha256: "2d98ce72530e71d617384963423820536f094501299e2cfec0a8d3c3fb6bcfb0" },
  { name: "SMK-37 Pro", version: "015", product: "SMK-37 Pro_015", size: 701140, sha256: "f7f1831cd7c9ad8b4831b6e71ea0bdbcdff9ae4c4077276b3c965511bf4d4fff" },
  { name: "SMK-37 Elite", version: "016", product: "SMK-37 Elite_016", size: 705236, sha256: "b2ee38abfef3ae922e4eecdb8a551e14345300ae6f270265feacd33b6ef99cb6" },
  { name: "SMK-37 Elite", version: "015", product: "SMK-37 Elite_015", size: 701140, sha256: "6cb061bda6fc4a285a1a1014e4fc30782278fa62bdc3add75d14b29d4d76253e" },
];
// M-VAVE's own download links (their server: never copied here)
export const STOCK_URLS = {
  "SMK-37 Pro": "https://yms-file-store.oss-cn-hongkong.aliyuncs.com/software/firmware/SMK-37Pro/SMK-37%20Pro_016.fwsc",
  "SMK-37 Elite": "https://yms-file-store.oss-cn-hongkong.aliyuncs.com/software/firmware/SMK-37Elite/SMK-37%20Elite_016.fwsc",
};
const MODELS = { "SMK-37 PRO": "SMK-37 Pro", "SMK-37 ELITE": "SMK-37 Elite" };

const printable = (m, i) => { const c = (m - i - 1) & 0xFF; return c >= 0x20 && c < 0x7F; };

// identity blocks: 20, or more when further identity markers are followed by unused 0x7D markers
function identityBlocks(f) {
  let n = BLOCKS;
  while (n < MAX_BLOCKS && (n + 1) * BLK <= f.length && f[n * BLK + KEEP] !== 0x7D && printable(f[n * BLK + KEEP], n)) n++;
  return n > BLOCKS && (n + 1) * BLK <= f.length && f[n * BLK + KEEP] === 0x7D ? n : BLOCKS;
}
// interleave blocks: the identity blocks, then more while the marker is the unused 0x7D (stock SMK-37 V016: 36)
function blockCount(f) {
  let n = identityBlocks(f);
  while (n < MAX_BLOCKS && (n + 1) * BLK <= f.length && f[n * BLK + KEEP] === 0x7D) n++;
  return n;
}
function deinterleave(f, blocks) {
  const out = new Uint8Array(f.length - blocks);
  for (let i = 0; i < blocks; i++) out.set(f.subarray(i * BLK, i * BLK + KEEP), i * KEEP);
  out.set(f.subarray(blocks * BLK), blocks * KEEP);
  return out;
}
function crc16(d, c = 0) {                 // CRC-16/XMODEM, as the package and the device use
  for (const b of d) { c ^= b << 8; for (let k = 0; k < 8; k++) c = c & 0x8000 ? ((c << 1) ^ 0x1021) & 0xFFFF : (c << 1) & 0xFFFF; }
  return c;
}
function jlDec(b) {                        // JieLi's header cipher (its own inverse)
  const out = Uint8Array.from(b);
  let key = 0xFFFF;
  for (let i = 0; i < out.length; i++) { out[i] ^= key & 0xFF; key = ((key << 1) ^ (key & 0x8000 ? 0x1021 : 0)) & 0xFFFF; }
  return out;
}
// the logical image's flash.bin (type 0) and ota.bin (type 100) match their CRCs
export function filesOk(img) {
  if (img.length < 0x40) return false;
  const h = jlDec(img.subarray(0, 0x40)), rd = (a, o, n) => a.slice(o, o + n).reduce((v, x, i) => v + x * 2 ** (8 * i), 0);
  if (crc16(h.subarray(2, 0x40)) !== rd(h, 0, 2)) return false;
  const count = rd(h, 8, 2);
  if (count < 1 || count > 8 || img.length < 0x40 + 0x50 * count) return false;
  for (let i = 0; i < count; i++) {
    const e = jlDec(img.subarray(0x40 + i * 0x50, 0x90 + i * 0x50));
    const typ = rd(e, 0, 2), off = rd(e, 8, 4), size = rd(e, 12, 4);
    if (off + size > img.length) return false;
    if ((typ === 0 || typ === 100) && crc16(img.subarray(off, off + size)) !== rd(e, 4, 2)) return false;
  }
  return true;
}
// the block count and the image (the detected count when the files' CRCs agree with it)
export function layout(f) {
  const n = blockCount(f);
  if (n !== BLOCKS) {
    const img = deinterleave(f, n);
    if (filesOk(img) || !filesOk(deinterleave(f, BLOCKS))) return { blocks: n, image: img };
  }
  return { blocks: BLOCKS, image: deinterleave(f, BLOCKS) };
}
export function productOfAny(f) {
  if (f.length < BLOCKS * BLK) throw new Error("too short");
  const ib = identityBlocks(f), n = ib === BLOCKS ? BLOCKS : Math.min(ib, layout(f).blocks);
  let s = "";
  for (let i = 0; i < n; i++) { const m = f[i * BLK + KEEP]; if (m !== 0x7D) s += String.fromCharCode((m - i - 1) & 0xFF); }
  return s;
}
async function sha256hex(bytes) {
  return [...new Uint8Array(await crypto.subtle.digest("SHA-256", bytes))].map((b) => b.toString(16).padStart(2, "0")).join("");
}
function bad(code, msg) { const e = new Error(msg); e.code = code; return e; }

// an official SMK-37 Pro / Elite package -> { name, version, product, model, image, known }; refused (code): "ours"
// (a JIANT or other community package, _4xx/_7xx/_8xx/_9xx), "damaged" (CRCs), "unknown"
export async function identifyStockPackage(bytes) {
  const sha = await sha256hex(bytes);
  let product, image;
  try { product = productOfAny(bytes); image = layout(bytes).image; }
  catch (_) { throw bad("unknown", "no es un firmware de M-VAVE para el SMK-37"); }
  const m = /^(.+)_(\d{3,6})$/.exec(product);
  const name = m && MODELS[m[1].toUpperCase()];
  if (m && name && !/^0\d\d$/.test(m[2])) throw bad("ours", "es un firmware de la comunidad (" + product + "), no el oficial");
  const k = STOCK_SMK.find((x) => x.size === bytes.length && x.sha256 === sha && x.product === product);
  if (!name) throw bad("unknown", "no es un firmware de M-VAVE para el SMK-37 Pro ni el Elite");
  if (!filesOk(image)) throw bad("damaged", "el archivo está dañado (sus CRC no coinciden)");
  return { name, version: m[2], product, model: m[1], image, known: Boolean(k) };
}
