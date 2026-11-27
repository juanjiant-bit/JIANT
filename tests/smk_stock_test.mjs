// SPDX-License-Identifier: GPL-3.0-only
// Studio's SMK-37 return to M-VAVE's firmware (web/studio/smk37/smkstock.js): a JIANT package read as fm1pkg.js reads it and
// refused as a community firmware; a package in the stock 36-block layout (made here from JIANT's image: nothing of
// M-VAVE's is in the repository) read back to the same image; a damaged one refused. Run: node tests/smk_stock_test.mjs
import fs from "fs";
import { identifyStockPackage, layout, filesOk, productOfAny } from "../web/studio/smk37/smkstock.js";
import { logicalImage, productOf } from "../web/fm1pkg.js";
const BLK = 0x30, KEEP = 0x2F;
function interleave(img, blocks, id) {
  const out = new Uint8Array(img.length + blocks);
  for (let i = 0; i < blocks; i++) {
    out.set(img.subarray(i * KEEP, (i + 1) * KEEP), i * BLK);
    out[i * BLK + KEEP] = i < id.length ? (id.charCodeAt(i) + i + 1) & 0xFF : 0x7D;
  }
  out.set(img.subarray(blocks * KEEP), blocks * BLK);
  return out;
}
let fails = 0; const ok = (w, c) => { console.log((c ? "ok   " : "FAIL ") + w); if (!c) fails++; };
for (const m of ["pro", "elite"]) {
  const ours = new Uint8Array(fs.readFileSync(new URL(`../web/studio/smk37/felucca-smk37-${m}.fwsc`, import.meta.url)));
  const img = logicalImage(ours);
  ok(`${m}: JIANT package's image passes the CRC check`, filesOk(img));
  ok(`${m}: smkstock reads JIANT's 20-block package as fm1pkg does`, productOfAny(ours) === productOf(ours) && layout(ours).blocks === 20 && Buffer.compare(Buffer.from(layout(ours).image), Buffer.from(img)) === 0);
  let refused = null; try { await identifyStockPackage(ours); } catch (e) { refused = e.code; }
  ok(`${m}: a JIANT package is refused as a community firmware (${refused})`, refused === "ours");
  const model = m === "pro" ? "SMK-37 Pro" : "SMK-37 Elite";
  const fake = interleave(img, 36, model + "_016");
  const k = await identifyStockPackage(fake);
  ok(`${m}: a 36-block stock-layout package: product ${k.product}, image identical`, k.product === model + "_016" && k.version === "016" && k.name === model && Buffer.compare(Buffer.from(k.image), Buffer.from(img)) === 0 && layout(fake).blocks === 36);
  const dmg = fake.slice(); dmg[dmg.length - 100] ^= 0xFF;
  let d = null; try { await identifyStockPackage(dmg); } catch (e) { d = e.code; }
  ok(`${m}: a damaged stock package is refused (${d})`, d === "damaged");
}
let u = null; try { await identifyStockPackage(new Uint8Array(4000)); } catch (e) { u = e.code; } ok(`garbage refused (${u})`, u === "unknown");
process.exit(fails ? 1 : 0);
