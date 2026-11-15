// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// (emu-transfer) The emulator <-> FM-1 transfers (web/emu/transfer.js) against a mocked Web MIDI FM-1 (fm1mock.mjs):
//   node web/emu/transfer_test.mjs                          the protocol model on both sides (no build needed)
//   node web/emu/transfer_test.mjs build/emu/felucca.wasm   then the firmware itself on both sides (the emulator's
//                                                           wasm, and a second instance as the FM-1 behind the mock)
// Checks: detection (version, USB / Bluetooth), the refusals of an older firmware, a project (sections and the index's
// part of that song only), a user preset (its bank record and its FM6 patch entry), everything, the verify after
// writing, the overwrite list asked about first, a cable pulled mid-transfer (said: which objects are new, which old;
// RETRY finishes it), a backup file of the emulator read by the editor's / installer's reader and restored byte-exact.
import fs from "fs";
import * as X from "./transfer.js";
import { jsDevice, wasmDevice, midiAccess, BACKUP_IDS_8 } from "./fm1mock.mjs";
import { edServe, edVersion } from "./worklet.js";

let fails = 0;
const check = (what, ok) => { console.log(`transfer: ${what.padEnd(84)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
const same = (a, b) => !!a && !!b && a.length === b.length && a.every((v, i) => v === b[i]);
const rejects = async (p) => { try { await p; return null; } catch (e) { return e; } };

// ---------------------------------------------------------------------------------------------- made-up contents
const rnd = (n, seed) => Uint8Array.from({ length: n }, (_, i) => (i * 131 + seed * 17 + (i >> 3)) & 255);
const le = (b, o, v) => { b[o] = v & 255; b[o + 1] = v >> 8 & 255; b[o + 2] = v >> 16 & 255; b[o + 3] = v >>> 24 & 255; };
function section(seed, name) {
  const b = rnd(3840, seed);
  b.fill(0, 3824, 3836);
  for (let i = 0; i < name.length; i++) b[3824 + i] = name.charCodeAt(i);
  return b;
}
function index(seed) {
  const b = rnd(X.IDX.SIZE, seed); le(b, 0, X.IDX.MAGIC); b[4] = 4; b[5] = 0; return b;
}
function record(name, engine = 0) {
  const r = new Uint8Array(192); r[0] = 0xA5; r[1] = 4; r[2] = engine; r[3] = 99;
  for (let i = 0; i < name.length; i++) r[4 + i] = name.charCodeAt(i);
  for (let i = 16; i < 192; i++) r[i] = (i * 7 + name.length) % 120;
  return r;
}
function bank(recs) { const b = X.emptyBank(); for (const [k, r] of recs) b.set(r, 8 + k * 192); return b; }
function upf(ents) { const b = X.emptyUpf(); let used = 0; for (const [k, seed] of ents) { b.set(rnd(116, seed), 16 + k * 116); used |= 1 << k; } le(b, 8, used); return b; }

function emuContent() {   /* the emulator's: PROJECT 1 (A, B, D), PROJECT 2 (A); presets 1 (an FM6 one with its patch), 5 */
  return new Map([[0, rnd(3840, 1)], [1, rnd(572, 2)], [10, index(3)],
    [40, section(4, "MY SONG")], [41, section(5, "")], [43, section(6, "")], [44, section(7, "SECOND")],
    [6, bank([[0, record("PAD ONE", 12)], [4, record("BASS 5", 0)]])], [9, upf([[0, 9]])]]);
}
function fm1Content() {   /* the FM-1's: PROJECT 3 (A..D), presets 20, 21; another index */
  return new Map([[0, rnd(3840, 11)], [1, rnd(572, 12)], [10, index(13)],
    [48, section(14, "OLD THREE")], [49, section(15, "")], [50, section(16, "")], [51, section(17, "")],
    [7, bank([[3, record("KEEP 20", 3)], [4, record("OLD 21", 5)]])], [9, upf([[20, 18]])]]);
}
const jsSide = (d, kind = "emu") => ({ kind, version: "JIANT V10.7.36", slow: 1, request: async ([cmd, a]) => {
  const r = d.handle(cmd, a); if (!r) throw new Error("Backup timed out"); return r; } });

// ---------------------------------------------------------------------------------------------- the protocol model
async function modelTests() {
  { /* detection: the FM-1 over USB; a Bluetooth port marked */
    const dev = jsDevice(fm1Content()), acc = midiAccess(dev);
    const c = await X.connectFM1(acc);
    check(`detect: the FM-1 over USB answers INFO (${c.info.version}, backup caps ${c.info.backupCaps})`, c.info.jiant && c.info.backupCaps === 3 && !c.bluetooth && c.info.version === "JIANT V10.7.36");
    c.link.close();
    const bt = await X.connectFM1(midiAccess(jsDevice(fm1Content()), { name: "FM-1 Bluetooth" }));
    check("detect: a port paired over Bluetooth is marked (EXPERIMENTAL) and gets longer waits", bt.bluetooth && bt.side.slow > 1);
    bt.link.close();
    const none = await rejects(X.connectFM1({ inputs: new Map(), outputs: new Map() }));
    check("detect: no FM-1 -> notfound", none?.code === "notfound");
    const order = X.candidatePorts({ inputs: new Map([["a", { name: "IAC Driver Bus 1", state: "connected" }], ["b", { name: "USB MIDI Interface", state: "connected" }], ["c", { name: "FM-1", state: "connected" }]]),
                                     outputs: new Map([["a", { name: "IAC Driver Bus 1", state: "connected" }], ["b", { name: "USB MIDI Interface", state: "connected" }], ["c", { name: "FM-1", state: "connected" }]]) });
    check("detect: virtual ports never probed, the FM-1's own port first", order.length === 2 && order[0].name === "FM-1");
  }
  { /* older firmware: refused with the reason, before anything is read or written */
    const emu = jsSide(jsDevice(emuContent())), em = await X.list(emu);
    const cases = [
      ["not JIANT (no backup restore)", { info: { version: "FELUCCA 1.0.2", jiant: false, backupCaps: 1 } }, "fwOld"],
      ["JIANT with 8 songs (before V10.6p, 43 objects)", { ids: BACKUP_IDS_8 }, "fwOld"],
      ["an older project format (its live music 3744 bytes, FUNA)", { small: true }, "fwOldFormat"]];
    for (const [what, o, code] of cases) {
      const content = fm1Content();
      if (o.small) content.set(0, rnd(3744, 1));
      const dev = jsDevice(content, o), c = await X.connectFM1(midiAccess(dev));
      const fl = await X.list(c.side), e = X.compat(c.info, fl, em, "to", "project");
      check(`old firmware: ${what} -> ${code}`, e?.code === code && dev.log.length === 0);
      c.link.close();
    }
    const content = fm1Content(); content.set(0, rnd(3744, 1));
    const c = await X.connectFM1(midiAccess(jsDevice(content)));
    check("old firmware: a user preset still goes to a FUNA FM-1 (the records' layout is the same)", !X.compat(c.info, await X.list(c.side), em, "to", "preset"));
    const olderEmu = emuContent(); olderEmu.set(0, rnd(3744, 1));
    const em2 = await X.list(jsSide(jsDevice(olderEmu))), c2 = await X.connectFM1(midiAccess(jsDevice(fm1Content())));
    check("a newer FM-1 than the emulator: GET FROM FM-1 refused (the emulator would not read it) -> emuOld", X.compat(c2.info, await X.list(c2.side), em2, "from", "project")?.code === "emuOld");
    c.link.close(); c2.link.close();
  }
  { /* catalogs and the question before overwriting */
    const emu = jsSide(jsDevice(emuContent())), dev = jsDevice(fm1Content()), c = await X.connectFM1(midiAccess(dev));
    const ec = await X.catalog(emu, ["ANALOG"]), fc = await X.catalog(c.side, ["ANALOG"]);
    check(`catalog: the emulator's PROJECT 1 "${ec.songs[0].name}" A B - D, PROJECT 2, presets 1 and 5`,
          ec.songs[0].name === "MY SONG" && ec.songs[0].stored.join() === "true,true,false,true" && ec.songs[1].name === "SECOND" &&
          !ec.songs[2].used && ec.presets[0]?.name === "PAD ONE" && ec.presets[4]?.name === "BASS 5" && !ec.presets[1]);
    check(`catalog: the FM-1's PROJECT 3 "${fc.songs[2].name}", presets 20 "${fc.presets[19]?.name}" and 21`, fc.songs[2].name === "OLD THREE" && fc.presets[19]?.name === "KEEP 20" && fc.presets[20]?.name === "OLD 21");
    const ow = X.overwrites(fc, { what: "project", to: 2 }), none = X.overwrites(fc, { what: "project", to: 3 });
    check("overwrite: PROJECT 3 on the FM-1 is asked about (its name), an empty PROJECT 4 is not", ow.length === 1 && ow[0].name === "OLD THREE" && none.length === 0);
    check("overwrite: preset 21 asked about; everything lists every stored project and preset", X.overwrites(fc, { what: "preset", to: 20 }).length === 1 &&
          X.overwrites(fc, { what: "all" }).length === 3);
    c.link.close();
  }
  { /* a project: PROJECT 1 of the emulator to PROJECT 3 of the FM-1 */
    const ed = jsDevice(emuContent()), emu = jsSide(ed), dev = jsDevice(fm1Content()), c = await X.connectFM1(midiAccess(dev));
    const before = dev.objs.get(10).slice(), steps = [];
    const r = await X.transfer(emu, c.side, { what: "project", from: 0, to: 2 }, (d, t) => steps.push([d, t]));
    const idx = dev.objs.get(10), src = ed.objs.get(10);
    const part = (b, s) => [[8, 36], [296, 128], [1320, 64], [1832, 64]].map(([o, n]) => Array.from(b.subarray(o + s * n, o + (s + 1) * n)).join()).join("|");
    let othersKept = true;
    for (const s of [0, 1, 3, 4, 5, 6, 7]) othersKept &&= part(idx, s) === part(before, s);
    check("project: sections A B D sent, C emptied (the slot as the emulator has it)", same(dev.objs.get(48), ed.objs.get(40)) && same(dev.objs.get(49), ed.objs.get(41)) &&
          dev.objs.get(50).length === 0 && same(dev.objs.get(51), ed.objs.get(43)));
    check("project: the index has PROJECT 3's rows, scenes, sources, variations from PROJECT 1; every other song's kept", part(idx, 2) === part(src, 0) && othersKept && idx[5] === before[5]);
    check(`project: verified (${r.verified.length} objects), the rows written last, progress to 100 %`, r.verified.length === 5 && dev.log.at(-1) === 10 && steps.at(-1)[0] === steps.at(-1)[1]);
    check("project: the FM-1's other objects untouched (live music, settings, presets)", same(dev.objs.get(0), fm1Content().get(0)) && same(dev.objs.get(1), fm1Content().get(1)) && same(dev.objs.get(7), fm1Content().get(7)));
    c.link.close();
  }
  { /* a preset: emulator 1 (FM6, a patch) to FM-1 21 (over "OLD 21"), then 5 (no patch) to 22 */
    const ed = jsDevice(emuContent()), emu = jsSide(ed), dev = jsDevice(fm1Content()), c = await X.connectFM1(midiAccess(dev));
    await X.transfer(emu, c.side, { what: "preset", from: 0, to: 20 });
    const b7 = dev.objs.get(7), u = dev.objs.get(9), used = (u[8] | u[9] << 8 | u[10] << 16 | u[11] << 24) >>> 0;
    check("preset: the record into bank 2's slot 5 (21), slot 20 kept", same(b7.subarray(8 + 4 * 192, 8 + 5 * 192), ed.objs.get(6).subarray(8, 200)) &&
          same(b7.subarray(8 + 3 * 192, 8 + 4 * 192), record("KEEP 20", 3)));
    check("preset: its FM6 patch entry to 21, 20's patch kept, the used bits", same(u.subarray(16 + 20 * 116, 16 + 21 * 116), ed.objs.get(9).subarray(16, 132)) && (used >>> 20 & 1) && dev.log.slice(-2).join() === "7,9");
    await X.transfer(emu, c.side, { what: "preset", from: 4, to: 21 });
    const u2 = dev.objs.get(9), used2 = (u2[8] | u2[9] << 8 | u2[10] << 16 | u2[11] << 24) >>> 0;
    check("preset: one without a patch clears the slot's entry", !(used2 >>> 21 & 1) && u2.subarray(16 + 21 * 116, 16 + 22 * 116).every((v) => !v) && (used2 >>> 20 & 1));
    const e = await rejects(X.transfer(emu, c.side, { what: "preset", from: 2, to: 0 }));
    check("preset: an empty slot is not sent (emptySlot), nothing written", e?.code === "emptySlot");
    const fresh = jsDevice(new Map([[0, rnd(3840, 1)], [1, rnd(572, 2)], [10, index(3)]])), c2 = await X.connectFM1(midiAccess(fresh));
    await X.transfer(emu, c2.side, { what: "preset", from: 0, to: 1 });
    check("preset: onto an FM-1 with no presets yet: a new bank and patch table made", fresh.objs.get(6).length === 3080 && fresh.objs.get(9).length === 3728);
    c.link.close(); c2.link.close();
  }
  { /* everything, the settings only when asked */
    const ed = jsDevice(emuContent()), emu = jsSide(ed), dev = jsDevice(fm1Content()), c = await X.connectFM1(midiAccess(dev));
    const r = await X.transfer(emu, c.side, { what: "all" });
    let songs = true;
    for (let id = 40; id < 64; id++) songs &&= same(dev.objs.get(id) || new Uint8Array(0), ed.objs.get(id) || new Uint8Array(0));
    check(`everything: every song, the index, presets, patches, the live music (${r.verified.length} verified)`, songs && same(dev.objs.get(10), ed.objs.get(10)) &&
          same(dev.objs.get(6), ed.objs.get(6)) && dev.objs.get(7).length === 0 && same(dev.objs.get(0), ed.objs.get(0)));
    check("everything: the FM-1's settings kept (its calibration), unless asked", same(dev.objs.get(1), fm1Content().get(1)));
    await X.transfer(emu, c.side, { what: "all", settings: true });
    check("everything + settings: the settings sent too", same(dev.objs.get(1), ed.objs.get(1)));
    c.link.close();
  }
  { /* the cable pulled during a project: said, which sections are new; RETRY finishes it */
    const ed = jsDevice(emuContent()), emu = jsSide(ed), dev = jsDevice(fm1Content());
    const acc = midiAccess(dev, { pullAfter: 20 }), c = await X.connectFM1(acc);
    const e = await rejects(X.transfer(emu, c.side, { what: "project", from: 0, to: 2 }));
    const parts = X.describeIds(e?.written || []);
    check(`dropped: "partial" (new: ${JSON.stringify(parts)}; still old: ${X.describeIds(e?.pending || []).map((p) => p.sections || p).join(" ")})`,
          e?.code === "partial" && e.lost && e.written.length >= 1 && e.pending.includes(10) && parts[0]?.song === 2);
    check("dropped: the half-sent section was never committed (the device writes at the commit only)", !dev.log.includes(e.pending[0]) && same(dev.objs.get(51), fm1Content().get(51)));
    c.link.close();
    acc.plug();
    const c2 = await X.connectFM1(acc), r = await X.transfer(emu, c2.side, { what: "project", from: 0, to: 2 });
    check("dropped: RETRY after plugging in again finishes it, verified", r.verified.length === 5 && same(dev.objs.get(51), ed.objs.get(43)));
    c2.link.close();
    const dev2 = jsDevice(fm1Content()), acc2 = midiAccess(dev2, { pullAfter: 3 }), c3 = await X.connectFM1(acc2);
    const e2 = await rejects(X.transfer(emu, c3.side, { what: "project", from: 0, to: 2 }));
    check("dropped before the first commit: \"lost\", nothing changed on the FM-1", e2?.code === "lost" && dev2.log.length === 0);
    c3.link.close();
  }
  { /* a reply lost once: the read is asked again; a write is not (aborted, said) */
    const ed = jsDevice(emuContent()), emu = jsSide(ed), dev = jsDevice(fm1Content());
    let lost = 0;
    const c = await X.connectFM1(midiAccess(dev, { loseReply: (cmd, a) => cmd === 66 && a[0] === 10 && !lost++ }));
    const r = await X.transfer(emu, c.side, { what: "project", from: 0, to: 3 });
    check("a lost GET reply is retried, the transfer completes", r.verified.length === 5 && lost >= 1);
    c.link.close();
  }
  { /* verify catches a device that stored something else */
    const ed = jsDevice(emuContent()), emu = jsSide(ed), dev = jsDevice(fm1Content());
    const h = dev.handle; dev.handle = (cmd, a) => { const r = h(cmd, a); if (cmd === 67 && a[0] === 2 && a[1] === 41) dev.objs.get(41)[7] ^= 1; return r; };
    const c = await X.connectFM1(midiAccess(dev)), e = await rejects(X.transfer(emu, c.side, { what: "project", from: 0, to: 0 }));
    check("verify: an object read back with another CRC -> \"verify\" naming it", e?.code === "verify" && e.bad?.includes(41));
    c.link.close();
  }
  { /* the emulator's backup file: the installer's / editor's reader takes it; import round trip byte-exact */
    const ed = jsDevice(emuContent()), emu = jsSide(ed);
    const file = await X.backupFile(emu), text = JSON.stringify(file), back = X.readBackup(text);
    check(`backup file: "${file.format}" v${file.version}, ${file.objects.length} objects, readBackup (the editor's restore) takes it`, back.objects.length === 35 && file.format === "felucca-backup");
    const ed2 = jsDevice(new Map([[0, rnd(3840, 30)], [1, rnd(572, 31)], [10, index(32)]])), emu2 = jsSide(ed2);
    const r = await X.restoreFile(emu2, text, { settings: true });
    const again = await X.backupFile(emu2);
    check(`import: the file into another emulator, every object byte-exact (${r.verified.length} verified)`, again.objects.every((o, i) => o.data === file.objects[i].data));
    const old = { ...file, objects: [...file.objects, ...Array.from({ length: 8 }, (_, i) => ({ id: 64 + i, size: 0, crc: 0, data: "" }))] };
    old.objects[old.objects.length - 1] = { id: 71, size: 3, crc: X.bkCrc(Uint8Array.of(1, 2, 3)), data: "AQID" };
    const r8 = await X.restoreFile(jsSide(jsDevice(new Map())), old);
    check(`import: an 8-song backup (before V10.6p): song 8 refused and said (${r8.songsLeft})`, r8.songsLeft.join() === "8");
  }
}

// ---------------------------------------------------------------------------------------------- the firmware itself
async function wasmTests(path) {
  const bytes = fs.readFileSync(path);
  const boot = async (seed) => {
    const { instance } = await WebAssembly.instantiate(bytes, {});
    const ex = instance.exports; ex._initialize(); ex.web_nor_erase(); ex.web_master(700); ex.web_seed(seed); ex.web_boot();
    for (let i = 0; i < 12; i++) ex.web_render(1024);
    return ex;
  };
  const emuEx = await boot(7), fmEx = await boot(8);
  if (!emuEx.web_ed) { check("the wasm has the backup commands (web_ed: transfer-api.h)", false); return; }
  emuEx.web_gen(6, 11, 60, 2);                           // a piece: TECHNO, FORM CLUB (sections and rows)
  for (let i = 0; i < 30; i++) emuEx.web_render(1024);
  const emu = X.emuSideFromExports(emuEx, edServe, edVersion(emuEx));
  const acc = midiAccess(wasmDevice(fmEx, edServe, "JIANT " + edVersion(fmEx)));
  const c = await X.connectFM1(acc);
  const em = await X.list(emu), fl = await X.list(c.side);
  check(`wasm: the emulator (${emu.version}) lists 35 objects, the FM-1 (${c.info.version}) too, same format`, em.length === 35 && fl.length === 35 && !X.compat(c.info, fl, em, "to", "all"));
  const ec = await X.catalog(emu);
  check(`wasm: the generator's piece is PROJECT 1 (sections ${ec.songs[0].stored.map((v, i) => v ? "ABCD"[i] : "-").join("")})`, ec.songs[0].used);
  const r = await X.transfer(emu, c.side, { what: "project", from: 0, to: 2 });
  const fm = await X.list(c.side), eml = await X.list(emu);
  let eq = true;
  for (let k = 0; k < 4; k++) eq &&= fm.byId.get(48 + k).crc === eml.byId.get(40 + k).crc && fm.byId.get(48 + k).size === eml.byId.get(40 + k).size;
  check(`wasm: PROJECT 1 -> the FM-1's PROJECT 3, the firmware took and verified it (${r.verified.length})`, eq && r.verified.includes(10));
  check("wasm: the FM-1 opens PROJECT 3 with the song's rows", fmEx.web_song_select(2) === 0 && (fmEx.web_render(4096), fmEx.web_song_rows() === emuEx.web_song_rows()) && fmEx.web_song_rows() >= 2);
  /* a user preset made in the emulator (a record of the firmware's layout, PUT as the editor's restore would) */
  const rec = record("WASM PAD", 0); rec[1] = 4; rec[3] = 99;
  for (let i = 16; i < 16 + 99; i++) rec[i] = 64;
  await X.putObject(emu, 6, bank([[2, rec]]));
  check("wasm: a user preset in the emulator's slot 3", emuEx.web_test_user_preset(2) === 1);
  await X.transfer(emu, c.side, { what: "preset", from: 2, to: 19 });
  check("wasm: -> the FM-1's slot 20 (its firmware validates the bank), verified", fmEx.web_test_user_preset(19) === 1 && !fmEx.web_test_user_preset(2));
  /* GET FROM FM-1: its PROJECT 3 into the emulator's PROJECT 5 */
  const r2 = await X.transfer(c.side, emu, { what: "project", from: 2, to: 4 });
  check("wasm: GET FROM FM-1: PROJECT 3 into the emulator's PROJECT 5, verified", r2.verified.length === 5);
  /* everything, then the file round trip on the firmware */
  const r3 = await X.transfer(emu, c.side, { what: "all" });
  check(`wasm: everything to the FM-1 (${r3.verified.length} objects verified)`, r3.verified.length >= 5);
  const file = await X.backupFile(emu), text = JSON.stringify(file);
  check(`wasm: DOWNLOAD BACKUP: ${file.objects.length} objects, the editor's reader takes it`, X.readBackup(text).objects.length === 35);
  const fresh = await boot(9), side2 = X.emuSideFromExports(fresh, edServe, edVersion(fresh));
  await X.restoreFile(side2, text, { settings: true });
  const again = await X.backupFile(side2);
  const stored = (o) => o.id >= 2;                       // (0 the music re-packed, 1 the settings re-exported)
  const diff = again.objects.filter((o, i) => stored(o) && o.data !== file.objects[i].data).map((o) => o.id);
  check(`wasm: IMPORT BACKUP into a fresh emulator: every stored object byte-exact${diff.length ? " (differ: " + diff + ")" : ""}`, !diff.length);
  check("wasm: and its settings and live music taken (sizes as the file's)", again.objects[1].size === file.objects[1].size && again.objects[0].size === file.objects[0].size);
  /* playing: a LIST stops it first (the firmware's ed_flash_stop, run by the emulator's own clock) */
  emuEx.web_play(1); for (let i = 0; i < 8; i++) emuEx.web_render(1024);
  const playing = emuEx.web_playing();
  await X.list(emu);
  check("wasm: the emulator playing: LIST stops it and answers", playing && !emuEx.web_playing());
  c.link.close();
}

await modelTests();
if (process.argv[2]) await wasmTests(process.argv[2]);
else console.log("transfer: (no felucca.wasm given: the firmware-on-both-sides checks skipped)");
console.log(fails ? `TRANSFER TESTS FAILED (${fails})` : "transfer: all ok");
process.exit(fails ? 1 : 0);
