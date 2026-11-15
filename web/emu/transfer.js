// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// (V10.7.36 emu-transfer) What you made in the browser emulator onto a real FM-1 and back, and the emulator's flash as a
// backup file. One protocol and one format on both sides: the editor's full backup (web/EDITOR_PROTOCOL.md "v7: full
// backup", BACKUP_LIST / GET / PUT, 65..67) and its file (fm1backup.js, "felucca-backup" v1, as the installer and the
// editor write and the editor restores). The emulator answers those commands with the firmware's own editor_backup.c
// (transfer-api.h, worklet.js edServe); the FM-1 over Web MIDI (fm1backup.js BackupConnection).
//   a project   = a song (SAVE > PROJECT 1..6): its four sections (ids 40 + 4 song .. 43 + 4 song) and its rows, scenes,
//                 track sources and variations in the song index (id 10, one object for every song: read, the song's
//                 part replaced, written back)
//   a preset    = a user preset slot (1..32): its record in its bank (id 6: 1..16, id 7: 17..32) and its FM6 patch /
//                 VOICE lyric entry (id 9), record first, patch second (as the device stores one: up_store)
//   everything  = every song and the index, the user presets and their patches, the music playing (id 0, RAM only),
//                 the settings (id 1) only when asked (they carry the FM-1's button calibration)
// Each object is committed by the device on its own (it checks the length, the CRC and the content first; nothing is
// written before the commit) and verified after by a new LIST: its size and CRC-32 as sent. A transfer that stops
// half-way says which objects are new, which are old, and how to finish (RETRY sends it all again).
// No DOM here: the page's panel is transfer-ui.js; node tests: transfer_test.mjs.

/* fm1backup.js sits next to this file in the built site (web/emu/build.sh copies it) and one level up in the tree */
const BK = await import("./fm1backup.js").catch(() => import("../fm1backup.js"));
export const { BACKUP_CMD, BACKUP_IDS, bkU32, bkR32, bkPack, bkUnpack, bkCrc, bkManifest, readBackup, captureBackup,
               BackupConnection, songsNotRestored } = BK;

export const NSONG = 6, NPRESET = 32, CHUNK = 256;
export const SONG_IDS = (s) => [40 + 4 * s, 41 + 4 * s, 42 + 4 * s, 43 + 4 * s];
const SECTION = "ABCD";

// ---------------------------------------------------------------------------------------------- errors
/* every error the panel shows carries a code (transfer-ui.js translates it) and a detail */
export class XferError extends Error {
  constructor(code, detail = "", extra = {}) { super(`${code}${detail ? ": " + detail : ""}`); this.code = code; this.detail = detail; Object.assign(this, extra); }
}
const RC_TEXT = { 1: "invalid", 2: "validation", 3: "playing", 4: "flash", 5: "stale" };
const isLink = (e) => e && !(e instanceof XferError) && /timed out|closed|busy|disconnect|lost|InvalidState/i.test(String(e.message || e) + (e.name || ""));

// ---------------------------------------------------------------------------------------------- INFO
/* the device's INFO reply (cmd 1): its version string, its engines' names, its tracks and the tagged blocks this file
 * needs: 42 01 caps (bit 0 backup read, bit 1 restore) and 4A 01 1 (JIANT). As web/index_pkg.html reads it */
export function parseInfo(a) {
  let pos = a.indexOf(0);
  if (pos < 0) throw new XferError("noinfo");
  const version = String.fromCharCode(...a.slice(0, pos)); pos++;
  const nengines = a[pos]; pos += 5;
  const engines = [];
  for (let i = 0; i < nengines; i++) {
    const end = a.indexOf(0, pos);
    if (end < 0) throw new XferError("noinfo");
    engines.push(String.fromCharCode(...a.slice(pos, end))); pos = end + 1;
  }
  const ntrk = a[pos], ext = a.slice(pos + 1);
  let backupCaps = 0, jiant = false;
  for (let i = 0; i + 2 < ext.length; i++) {
    if (ext[i] === 0x42 && ext[i + 1] === 1 && !backupCaps) backupCaps = ext[i + 2];
    if (ext[i] === 0x4A && ext[i + 1] === 1 && ext[i + 2] === 1) jiant = true;
  }
  return { version, engines, ntrk, backupCaps, jiant: jiant || /^JIANT /i.test(version) };
}

// ---------------------------------------------------------------------------------------------- the two sides
/* A side: { kind: "emu" | "fm1", request([cmd, args], opt) -> reply body, version, slow }. The emulator's requests go
 * to edServe (worklet.js) through a MessagePort, or straight into the exports (node) */
export function emuSideFromExports(ex, edServe, version = "") {
  return { kind: "emu", version, slow: 1, request: async ([cmd, args]) => {
    const r = edServe(ex, [cmd, ...args]);
    if (!r) throw new Error("Backup timed out (the emulator sent no reply)");
    return r;
  } };
}
export function emuSideFromPort(port, timeout = 8000) {
  let seq = 0;
  const waiting = new Map();
  const side = { kind: "emu", version: "", slow: 1 };
  port.onmessage = (e) => {
    const m = e.data || {};
    if (m.version) side.version = m.version;
    const w = waiting.get(m.seq);
    if (!w) return;
    waiting.delete(m.seq); clearTimeout(w.t);
    if (m.reply) w.res(m.reply); else w.rej(new Error(m.error || "Backup timed out (the emulator sent no reply)"));
  };
  port.start?.();
  side.request = ([cmd, args]) => new Promise((res, rej) => {
    const s = ++seq, t = setTimeout(() => { waiting.delete(s); rej(new Error("Backup timed out (the emulator)")); }, timeout);
    waiting.set(s, { res, rej, t });
    port.postMessage({ seq: s, req: [cmd, ...args] });
  });
  return side;
}

/* the FM-1's MIDI ports: every input with an output of the same name, the FM-1's first (its own USB name), then
 * the others that could be it (a USB hub's, a Bluetooth pairing's); never the browser's or the OS's virtual ports */
const NOT_FM1 = /wavetable|iac driver|midi through|loopmidi|loopbe|virtual|rtpmidi|network session|emulator/i;
export const isBluetooth = (p) => /bluetooth|\bble\b|\bbt\b|wireless/i.test(`${p.name || ""} ${p.manufacturer || ""}`);
export function candidatePorts(access) {
  const outs = [...access.outputs.values()], out = [];
  for (const input of access.inputs.values()) {
    if (input.state === "disconnected" || NOT_FM1.test(input.name || "")) continue;
    const output = outs.find((o) => o.name === input.name && o.state !== "disconnected");
    if (output) out.push({ input, output, name: input.name || "MIDI", bluetooth: isBluetooth(input) || isBluetooth(output) });
  }
  const rank = (c) => (/fm-1|jiant|felucca/i.test(c.name) ? 0 : 2) + (c.bluetooth ? 1 : 0);
  return out.sort((a, b) => rank(a) - rank(b));
}
/* the first candidate that answers INFO as an FM-1 does: { side, info, name, bluetooth, link }. A slow link (Bluetooth)
 * gets longer waits for every request */
export async function connectFM1(access, opt = {}) {
  const tried = [];
  for (const c of candidatePorts(access)) {
    try { await c.input.open?.(); await c.output.open?.(); } catch (_) { continue; }
    const link = new BackupConnection(c.input, c.output);
    try {
      const info = parseInfo(await link.request([1, []], { timeout: c.bluetooth ? 4000 : 1500, retries: 1 }));
      const slow = c.bluetooth ? 3 : 1;
      const side = { kind: "fm1", version: info.version, slow, bluetooth: c.bluetooth, name: c.name,
                     request: (r, o = {}) => link.request(r, { ...o, timeout: (o.timeout || 1200) * slow }) };
      return { side, info, name: c.name, bluetooth: c.bluetooth, link };
    } catch (e) { link.close(); tried.push(c.name); }
  }
  throw new XferError("notfound", tried.join(", "));
}

// ---------------------------------------------------------------------------------------------- reading
const LIST_OPT = { timeout: 8000, retries: 1 };
/* the device's objects now: [{id, size, crc}] (the LIST stops its playback and takes the snapshot GET reads) */
export async function list(side) {
  const a = await side.request([BACKUP_CMD.LIST, []], LIST_OPT);
  if (a[0] === 1 && a[1] === 3) throw new XferError("playing");
  const m = bkManifest(a);
  m.byId = new Map(m.map((o) => [o.id, o]));
  return m;
}
/* bytes off .. off + n of object id (in 256-byte pieces); a late reply to an earlier try is skipped */
export async function getBytes(side, id, off, n, onBytes = () => {}) {
  const out = new Uint8Array(n);
  for (let p = 0; p < n; p += CHUNK) {
    const size = Math.min(CHUNK, n - p), at = off + p;
    const match = (r) => r[0] === id && r.length >= 7 && (() => { try { return bkR32(r, 2) === at; } catch (_) { return false; } })();
    const a = await side.request([BACKUP_CMD.GET, [id, ...bkU32(at), size & 127, size >>> 7]], { timeout: 2500, retries: 2, match });
    if (a[1]) throw new XferError(a[1] === 5 ? "stale" : a[1] === 3 ? "playing" : "readfail", `${id} rc ${a[1]}`);
    if (a[0] !== id || bkR32(a, 2) !== at || (a[7] | a[8] << 7) !== size) throw new XferError("readfail", `${id}`);
    out.set(bkUnpack(a.slice(9), size), p);
    onBytes(size);
  }
  return out;
}
/* a whole object as the LIST named it, its CRC checked */
export async function getObject(side, o, onBytes) {
  const bytes = o.size ? await getBytes(side, o.id, 0, o.size, onBytes) : new Uint8Array(0);
  if (bkCrc(bytes) !== o.crc) throw new XferError("changed", `${o.id}`);
  return bytes;
}
/* a project's name: the 12 bytes before the store's hash (project.c PROJ_NAME_OFF; every format), as proj_name_get */
export function nameOf(tail) {
  let s = "";
  for (const b of tail) {
    if (!b) break;
    if (b < 32 || b > 126) return "";
    s += String.fromCharCode(b >= 97 && b <= 122 ? b - 32 : b);
  }
  return s;
}
// the user preset banks (upreset.c up_bank_t: magic "UPB1", rsize 192, nslot 16, then 16 records) and their patches
// (up_fm6.c upf_t: magic "UPF6", ver 1, nslot 32, used bits, rsv, then 32 x {tag u32, 112 packed bytes})
export const UP = { BANK: 8 + 16 * 192, REC: 192, MAGIC: 0x31425055, USED: 0xA5 };
export const UPF = { SIZE: 16 + 32 * 116, ENT: 116, MAGIC: 0x36465055 };
const le32 = (b, o) => (b[o] | b[o + 1] << 8 | b[o + 2] << 16 | b[o + 3] << 24) >>> 0;
const put32 = (b, o, v) => { b[o] = v & 255; b[o + 1] = v >>> 8 & 255; b[o + 2] = v >>> 16 & 255; b[o + 3] = v >>> 24 & 255; };
export function emptyBank() {
  const b = new Uint8Array(UP.BANK); put32(b, 0, UP.MAGIC); b[4] = UP.REC; b[5] = 0; b[6] = 16; b[7] = 0; return b;
}
export function emptyUpf() {
  const b = new Uint8Array(UPF.SIZE); put32(b, 0, UPF.MAGIC); b[4] = 1; b[6] = 32; return b;
}
const bankOk = (b) => b.length === UP.BANK && le32(b, 0) === UP.MAGIC && (b[4] | b[5] << 8) === UP.REC && (b[6] | b[7] << 8) === 16;
const upfOk = (b) => b.length === UPF.SIZE && le32(b, 0) === UPF.MAGIC && (b[4] | b[5] << 8) === 1 && (b[6] | b[7] << 8) === 32;
/* a preset record (192 bytes) as the panel lists it, or null when the slot is empty (upreset.c up_valid, in short) */
export function presetOf(rec, engines = []) {
  if (!rec || rec[0] !== UP.USED || !rec[1] || rec[1] > 5 || !rec[4]) return null;
  let name = "";
  for (let i = 4; i < 16 && rec[i]; i++) name += String.fromCharCode(rec[i]);
  return { name, engine: rec[2], engineName: engines[rec[2]] || `ENG ${rec[2]}` };
}
/* what a side holds, for the pickers: its songs (used, name of section A, the sections stored) and its 32 presets */
export async function catalog(side, engines = [], m = null) {
  m = m || await list(side);
  const songs = [];
  for (let s = 0; s < NSONG; s++) {
    const secs = SONG_IDS(s).map((id) => m.byId.get(id));
    const stored = secs.map((o) => !!(o && o.size));
    let name = "";
    const a = secs.find((o) => o && o.size >= 16);
    if (a) name = nameOf(await getBytes(side, a.id, a.size - 16, 12));
    songs.push({ n: s, used: stored.some(Boolean), stored, name });
  }
  const presets = [];
  for (const id of [6, 7]) {
    const o = m.byId.get(id), bank = o && o.size ? await getObject(side, o) : null;
    for (let k = 0; k < 16; k++) presets.push(bank && bankOk(bank) ? presetOf(bank.subarray(8 + k * UP.REC, 8 + (k + 1) * UP.REC), engines) : null);
  }
  return { songs, presets, manifest: m };
}

// ---------------------------------------------------------------------------------------------- format checks
/* null, or why this FM-1 cannot take / give what the emulator has: not JIANT or no restore (an older or other
 * firmware), another number of songs (JIANT before V10.6p kept 8), an older project format than the emulator's (its
 * firmware would refuse the sections: update it with the installer). dir "to": emulator -> FM-1; "from": FM-1 -> emulator */
export function compat(info, fm1List, emuList, dir, what) {
  if (!info || !info.jiant || (info.backupCaps & 3) !== 3) return new XferError("fwOld", info ? info.version : "");
  if (fm1List.length !== BACKUP_IDS.length) return new XferError("fwOld", info.version);
  const f0 = fm1List.byId.get(0)?.size || 0, e0 = emuList.byId.get(0)?.size || 0;
  if (what !== "preset") {
    if (dir === "to" && f0 < e0) return new XferError("fwOldFormat", info.version);
    if (dir === "from" && f0 > e0) return new XferError("emuOld", info.version);
  }
  return null;
}

// ---------------------------------------------------------------------------------------------- writing
/* one object, begin / 256-byte pieces / commit; a failed piece aborts it (nothing written: the device writes only at
 * its commit). rc 1 at begin: the device does not take this object (an older firmware's): { refused: true } */
export async function putObject(side, id, bytes, onBytes = () => {}) {
  const ask = async (args) => {
    const a = await side.request([BACKUP_CMD.PUT, args], { timeout: 4000, retries: 0 });
    if (a[1] !== args[1] && a[1] !== 127) throw new XferError("writefail", `${id}`);
    return a[2];
  };
  let rc = await ask([0, id, ...bkU32(bytes.length), ...bkU32(bkCrc(bytes))]);
  if (rc === 1) return { refused: true };
  if (rc) throw new XferError(RC_TEXT[rc] || "writefail", `${id} rc ${rc}`);
  try {
    for (let off = 0; off < bytes.length; off += CHUNK) {
      const piece = bytes.subarray(off, off + CHUNK);
      rc = await ask([1, id, ...bkU32(off), ...bkPack(piece)]);
      if (rc) throw new XferError(RC_TEXT[rc] || "writefail", `${id} rc ${rc}`);
      onBytes(piece.length);
    }
    rc = await ask([2, id]);
    if (rc) throw new XferError(RC_TEXT[rc] || "writefail", `${id} rc ${rc}`);
  } catch (e) {
    await ask([3, id]).catch(() => {});
    throw e;
  }
  return { refused: false };
}

// ---------------------------------------------------------------------------------------------- the song index
// song_chain.c song_index_t (version 4, 2344 bytes): magic, version, cur, rsv[2]; rows[8] (36 bytes: count, rsv[3],
// 16 x {slot, bars}); scene[8][16] (8 bytes); tsrc[8][16] (u32); tvar[8][16] (u32). Room for 8 songs, 6 used
export const IDX = { SIZE: 2344, MAGIC: 0x474E4F53, V: 4, ROWS: 8, ROW: 36, SCENE: 296, SCENE_N: 128, TSRC: 1320, TVAR: 1832, VAR_N: 64 };
/* an index of any version the firmware takes (2: 1320 bytes, 3: 1832, 4) as version 4: the parts missing empty */
export function indexV4(b) {
  if (!b || b.length < 8 || le32(b, 0) !== IDX.MAGIC) throw new XferError("badIndex");
  if (b.length === IDX.SIZE && b[4] === IDX.V) return b.slice();
  if ((b.length === IDX.TSRC && b[4] === 2) || (b.length === IDX.TVAR && b[4] === 3)) {
    const o = new Uint8Array(IDX.SIZE); o.set(b); o[4] = IDX.V; return o;
  }
  throw new XferError("badIndex", `${b.length}`);
}
/* dst's index with song d's rows, scenes, track sources and variations taken from song s of src (cur kept) */
export function mergeIndex(dst, d, src, s) {
  const o = indexV4(dst), from = indexV4(src);
  for (const [at, n] of [[IDX.ROWS, IDX.ROW], [IDX.SCENE, IDX.SCENE_N], [IDX.TSRC, IDX.VAR_N], [IDX.TVAR, IDX.VAR_N]]) {
    const base = at === IDX.ROWS ? 8 : at;
    o.set(from.subarray(base + s * n, base + (s + 1) * n), base + d * n);
  }
  return o;
}

// ---------------------------------------------------------------------------------------------- the jobs
/* job: { what: "project", from: s, to: d } | { what: "preset", from: k, to: j } | { what: "all", settings: bool,
 * live: bool }. src and dst are sides. onProgress(done, total, phase). Returns { written: [ids], verified: [ids],
 * refused: [ids] }. On a failure after a first commit: an XferError "partial" with .written (the objects already new)
 * and .pending (the ones still old), .cause (the error) */
export async function transfer(src, dst, job, onProgress = () => {}) {
  let done = 0, total = 1;
  const tick = (n, phase) => { done += n; onProgress(Math.min(done, total), total, phase); };
  const sm = await list(src);
  const plan = [];                                     // [id, bytes]
  if (job.what === "project") {
    const ids = SONG_IDS(job.from), dsts = SONG_IDS(job.to);
    total = ids.reduce((n, id) => n + 2 * (sm.byId.get(id)?.size || 0), 0) + 3 * IDX.SIZE;
    for (let k = 0; k < 4; k++) plan.push([dsts[k], await getObject(src, sm.byId.get(ids[k]), (n) => tick(n, "read"))]);
    const sIdx = await getObject(src, sm.byId.get(10), (n) => tick(n, "read"));
    const dm = await list(dst);
    const dIdx = await getObject(dst, dm.byId.get(10), (n) => tick(n, "read"));
    plan.push([10, mergeIndex(dIdx, job.to, sIdx, job.from)]);   // (the rows last: they name the sections)
  } else if (job.what === "preset") {
    const sb = 6 + (job.from >> 4), db = 6 + (job.to >> 4);
    total = 4 * (UP.BANK + UPF.SIZE);
    const sBank = await getObject(src, sm.byId.get(sb), (n) => tick(n, "read"));
    const sUpf = await getObject(src, sm.byId.get(9), (n) => tick(n, "read"));
    const rec = bankOk(sBank) ? sBank.subarray(8 + (job.from & 15) * UP.REC, 8 + ((job.from & 15) + 1) * UP.REC) : null;
    if (!rec || !presetOf(rec)) throw new XferError("emptySlot", `${job.from + 1}`);
    const dm = await list(dst);
    let dBank = await getObject(dst, dm.byId.get(db), (n) => tick(n, "read"));
    let dUpf = await getObject(dst, dm.byId.get(9), (n) => tick(n, "read"));
    dBank = bankOk(dBank) ? dBank.slice() : emptyBank();
    dUpf = upfOk(dUpf) ? dUpf.slice() : emptyUpf();
    dBank.set(rec, 8 + (job.to & 15) * UP.REC);
    const sUsed = upfOk(sUpf) ? le32(sUpf, 8) : 0, has = (sUsed >>> job.from) & 1;
    const at = 16 + job.to * UPF.ENT;
    if (has) dUpf.set(sUpf.subarray(16 + job.from * UPF.ENT, 16 + (job.from + 1) * UPF.ENT), at);
    else dUpf.fill(0, at, at + UPF.ENT);
    put32(dUpf, 8, has ? le32(dUpf, 8) | (1 << job.to) : le32(dUpf, 8) & ~(1 << job.to));
    plan.push([db, dBank], [9, dUpf]);                 // (the record first, its patch second: up_store's order)
  } else if (job.what === "all") {
    const ids = [6, 7, 9, 10, ...Array.from({ length: 24 }, (_, i) => 40 + i)];
    if (job.settings) ids.push(1);
    if (job.live !== false) ids.push(0);               // (the music playing: RAM only, last)
    total = ids.reduce((n, id) => n + 2 * (sm.byId.get(id)?.size || 0), 0);
    for (const id of ids) plan.push([id, await getObject(src, sm.byId.get(id), (n) => tick(n, "read"))]);
  } else throw new XferError("badJob", String(job.what));
  return writePlan(dst, plan, tick);
}

/* the plan's objects into dst, then a LIST: each stored object's size and CRC as sent (ids 0 and 1 are not stored as
 * sent: the music playing is re-packed, the settings re-exported; the device checked them at the commit) */
async function writePlan(dst, plan, tick) {
  const written = [], refused = [];
  try {
    for (const [id, bytes] of plan) {
      const r = await putObject(dst, id, bytes, (n) => tick(n, "write"));
      if (r.refused) { refused.push(id); tick(bytes.length, "write"); } else written.push(id);
    }
  } catch (e) {
    const pending = plan.map(([id]) => id).filter((id) => !written.includes(id) && !refused.includes(id));
    if (!written.length) throw isLink(e) ? new XferError("lost", e.message, { written, pending, cause: e }) : e;
    throw new XferError("partial", e.code || e.message, { written, pending, cause: e, lost: isLink(e) });
  }
  const m = await list(dst), verified = [], bad = [];
  for (const [id, bytes] of plan) {
    if (!written.includes(id)) continue;
    if (id === 0 || id === 1) { verified.push(id); continue; }
    const o = m.byId.get(id);
    if (o && o.size === bytes.length && o.crc === bkCrc(bytes)) { verified.push(id); continue; }
    /* the song index: the device keeps its own current song (cur, byte 5) through a restore: read back, compared
     * without that byte */
    if (id === 10 && o && o.size === bytes.length) {
      const back = await getObject(dst, o), a = back.slice(), b = bytes.slice(); a[5] = b[5] = 0;
      if (a.every((v, i) => v === b[i])) { verified.push(id); continue; }
    }
    bad.push(id);
  }
  if (bad.length) throw new XferError("verify", bad.join(", "), { written, pending: [], bad });
  return { written, verified, refused };
}

/* a whole backup file (fm1backup.js readBackup: any archive the editor restores, 8-song ones too) into a side, as
 * the editor restores it (the songs, the presets, then the settings when asked, the live music last), each object
 * verified. Ids the side does not take (an 8-song archive's songs 7, 8; the retired bank 8) are left out and said */
export async function restoreFile(dst, file, opt = {}, onProgress = () => {}) {
  const archive = readBackup(file);
  const by = new Map(archive.objects.map((o) => [o.id, o]));
  const ids = archive.objects.map((o) => o.id).filter((id) => id >= 6 && id !== 8);
  if (opt.settings) ids.push(1);
  if (opt.live !== false) ids.push(0);
  const plan = ids.map((id) => [id, by.get(id).bytes]);
  let done = 0;
  const total = plan.reduce((n, [, b]) => n + b.length, 0);
  const r = await writePlan(dst, plan, (n) => { done += n; onProgress(Math.min(done, total), total, "write"); });
  return { ...r, archive, songsLeft: songsNotRestored(r.refused.filter((id) => by.get(id).size)) };   // (with data only)
}

/* a side's complete backup file: exactly what the installer's and the editor's "Back up" write (captureBackup) */
export async function backupFile(side, onProgress) {
  return captureBackup((r, o) => side.request(r, o), side.version || "", onProgress);
}
export const backupName = (who, d = new Date()) => `JIANT-${who === "emu" ? "emu-" : ""}backup-${d.toISOString().slice(0, 10)}.json`;
/* the objects a job will overwrite on dst that hold something, for the question before writing */
export function overwrites(dstCat, job) {
  if (job.what === "project") return dstCat.songs[job.to].used ? [{ kind: "project", n: job.to, name: dstCat.songs[job.to].name }] : [];
  if (job.what === "preset") return dstCat.presets[job.to] ? [{ kind: "preset", n: job.to, name: dstCat.presets[job.to].name }] : [];
  return [...dstCat.songs.filter((s) => s.used).map((s) => ({ kind: "project", n: s.n, name: s.name })),
          ...dstCat.presets.map((p, n) => p && { kind: "preset", n, name: p.name }).filter(Boolean)];
}
/* the ids of a "partial" error as the person reads them: "PROJECT 3 sections A, B", "the song rows", "preset bank" */
export function describeIds(ids) {
  const out = [];
  const secs = new Map();
  for (const id of ids) {
    if (id >= 40 && id < 64) { const s = (id - 40) >> 2; secs.set(s, (secs.get(s) || "") + SECTION[(id - 40) & 3]); }
    else out.push({ 0: "live", 1: "settings", 6: "bank1", 7: "bank2", 9: "patches", 10: "rows" }[id] || `#${id}`);
  }
  return [...[...secs].map(([s, k]) => ({ song: s, sections: k })), ...out];
}
