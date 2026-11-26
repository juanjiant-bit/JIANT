/* JIANT FM's old projects (before V10.7) brought up to date. From V10.8.51 the FM-1 no longer migrates them ("OLD
 * PROJECT: OPEN IN STUDIO"). A project is a backup object (song s 0..5, section k A..D: id 40 + 4s + k; 2..5 are the
 * current song's same data and are skipped). The current firmware's wasm says which are old (web_proj_load rc 3); the
 * converter (/JIANT/emu-convert/, V10.8.50 with web_proj_load / web_proj_save) loads one and saves it in a format today's
 * firmware reads. Only the old ones change: a new one re-saved by V10.8.50 would lose what came after it.
 * No audio: the wasm is instantiated in the page, booted, and used as a function */
import { bkU32, bkPack, bkCrc, BACKUP_CMD } from "./fm1backup.js";
export const PROJECT_IDS = Array.from({ length: 24 }, (_, i) => 40 + i);
export const projectName = (id) => `${Math.floor((id - 40) / 4) + 1}${"ABCD"[(id - 40) % 4]}`;   // "2B": song 2, part B

const cache = new Map();
async function boot(url) {
  if (cache.has(url)) return cache.get(url);
  const p = (async () => {
    const r = await fetch(url, { cache: "no-cache" }); if (!r.ok) throw new Error(`wasm ${r.status}`);
    const { instance } = await WebAssembly.instantiate(await r.arrayBuffer(), { env: { emscripten_notify_memory_growth() {} } });
    const ex = instance.exports;
    if (!ex.web_proj_load || !ex.web_bk_buf) return null;           // a build without them
    ex._initialize(); ex.web_nor_erase(); ex.web_master(700); if (ex.web_seed) ex.web_seed(1); ex.web_boot();
    return ex;
  })();
  cache.set(url, p);
  try { return await p; } catch (e) { cache.delete(url); throw e; }
}
function load(ex, bytes) {
  const max = ex.web_bk_max ? ex.web_bk_max() : 4096;
  if (bytes.length > max) return 2;
  new Uint8Array(ex.memory.buffer, ex.web_bk_buf(), bytes.length).set(bytes);
  return ex.web_proj_load(0, bytes.length);
}
/* the firmware of today: the first of these URLs whose wasm can tell (V10.8.51 or later) */
export async function checker(urls) {
  for (const u of urls) { try { const ex = await boot(u); if (ex) return ex; } catch (_) {} }
  return null;
}
/* each project object: "ok" (today's firmware opens it), "old" (convert it), "bad" (not a project), "empty" */
export function classify(ex, objects) {
  return objects.filter((o) => PROJECT_IDS.includes(o.id)).map((o) => {
    if (!o.bytes || !o.bytes.length) return { id: o.id, state: "empty" };
    const rc = load(ex, o.bytes);
    return { id: o.id, state: rc === 0 ? "ok" : rc === 3 ? "old" : "bad", rc };
  });
}
export async function convert(url, bytes) {
  const ex = await boot(url); if (!ex || !ex.web_proj_save) throw new Error("converter");
  const rc = load(ex, bytes); if (rc) throw new Error(`load ${rc}`);
  const n = ex.web_proj_save(); if (!n) throw new Error("save");
  return new Uint8Array(ex.memory.buffer, ex.web_bk_buf(), n).slice();
}
/* one object written back (BACKUP_PUT: begin, the chunks, commit; abort on a failure) */
export async function putObject(request, id, bytes) {
  const ask = async (args) => { const a = await request([BACKUP_CMD.PUT, args], { timeout: 4000, retries: 0 }); if (a[2]) throw new Error(`put rc ${a[2]}`); return a; };
  await ask([0, id, ...bkU32(bytes.length), ...bkU32(bkCrc(bytes))]);
  try {
    for (let off = 0; off < bytes.length; off += 256) await ask([1, id, ...bkU32(off), ...bkPack(bytes.subarray(off, off + 256))]);
    await ask([2, id]);
  } catch (e) { await ask([3, id]).catch(() => {}); throw e; }
}
/* an archive file's object replaced (data base64, size, crc), for a converted file to download */
export function replaceInFile(file, id, bytes) {
  let s = ""; for (const b of bytes) s += String.fromCharCode(b);
  file.objects = file.objects.map((o) => (o.id === id ? { ...o, size: bytes.length, crc: bkCrc(bytes), data: btoa(s) } : o));
}
