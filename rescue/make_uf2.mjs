#!/usr/bin/env node
// SPDX-License-Identifier: MIT
// Command-line version of fm1-rescue.html:
//   node rescue/make_uf2.mjs BASE.uf2 FM-1.fwsc wl82loader.bin OUT.uf2 [REGION_OUT.bin]
// REGION_OUT.bin (optional): the V15 region 0x4000..0x92FFF, for test/run.sh.
import { readFileSync, writeFileSync } from "node:fs";
import { buildRescueUf2, checkPackage } from "./builder.js";

const [base, fwsc, loader, out, regionOut] = process.argv.slice(2);
if (!out) {
  console.error("usage: node make_uf2.mjs BASE.uf2 FM-1.fwsc wl82loader.bin OUT.uf2 [REGION_OUT.bin]");
  process.exit(2);
}
try {
  const pkg = new Uint8Array(readFileSync(fwsc));
  const r = await buildRescueUf2({
    baseUf2: new Uint8Array(readFileSync(base)),
    fwsc: pkg,
    loader: new Uint8Array(readFileSync(loader)),
  });
  writeFileSync(out, r.uf2);
  if (regionOut) writeFileSync(regionOut, (await checkPackage(pkg)).region);
  console.log(`${out}: ${r.uf2.length} bytes (${r.identity}${r.official ? ", official V15" : ", JIANT"})`);
} catch (e) {
  console.error("refused: " + e.message);
  process.exit(1);
}
