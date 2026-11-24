// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// JIANT Studio's checks without a browser (node web/studio/studio_test.mjs):
// - studio-lib.js's screen frames are the editor's (editor.html scrEncode / scrDecode: the same bytes, the same pixels)
//   and the firmware's (a frame tests/editor_test.c's scr_tap sends);
// - WAV (mono, stereo, 8 channels), ZIP (stored, CRC), Standard MIDI File (header, delta times, running notes);
// - every text the page and studio.js use, in Spanish and English; the views and their sections.
import { readFileSync } from "node:fs";
import { join } from "node:path";
import vm from "node:vm";
import { scrDecode, scrEncode, wavFile, zipStored, midiFile, crc32, SCR } from "./studio-lib.js";

const HERE = new URL(".", import.meta.url).pathname;
let failed = 0;
const ok = (c, what) => { console.log(`studio: ${what.padEnd(72)} ${c ? "ok" : "FAIL"}`); if (!c) failed++; };
const eq = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);

/* ---- the screen: the editor's own functions, from its protocol section */
const html = readFileSync(join(HERE, "..", "editor.html"), "utf8");
const proto = html.slice(html.indexOf("/*PROTO-BEGIN*/"), html.indexOf("/*PROTO-END*/"));
const E = vm.runInNewContext(proto + ";({ scrEncode, scrDecode })", { setTimeout, clearTimeout, setInterval, clearInterval, console });
const img = new Uint16Array(240 * 240);
for (let i = 0, s = 3; i < img.length; i++) { s = (Math.imul(s, 1103515245) + 12345) >>> 0; img[i] = i % 5 < 2 ? 0x2104 : (i >> 9) % 3 ? s >>> 16 : 0xF800; }
const mine = scrEncode(img, 0, 0, 240, 240), theirs = E.scrEncode(img, 0, 0, 240, 240);
ok(mine.length === theirs.length && mine.every((f, k) => eq(f, theirs[k])), `the screen frames are the editor's, byte for byte (${mine.length} frames)`);
const a = new Uint16Array(240 * 240), b = new Uint16Array(240 * 240);
for (const f of mine) { scrDecode(f, a); E.scrDecode(f, b); }
ok(eq(a, img) && eq(b, img), "decoded by Studio and by the editor: the picture, every pixel");
/* a frame from the firmware (tests/editor_test.c's scr_tap: a 24 x 5 strip at 3, 7, the same seed as web/test_web.mjs) */
const FW = (() => { const t = readFileSync(join(HERE, "..", "test_web.mjs"), "utf8"); const m = /const FW = \[([0-9,]+)\]/.exec(t); return m ? m[1].split(",").map(Number) : null; })();
if (FW) {
  const strip = new Uint16Array(24 * 5);
  for (let i = 0, seed = 7; i < strip.length; i++) { seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0; strip[i] = i < 30 ? 0x1234 : (seed >>> 28) < 6 ? seed >>> 16 : (i & 1 ? 0xF800 : 0x07E0); }
  const fb = new Uint16Array(240 * 240), box = scrDecode(FW.slice(5, -1), fb);
  let same = box && box.x === 3 && box.y === 7;
  for (let j = 0; j < 5; j++) for (let i = 0; i < 24; i++) same &&= fb[(7 + j) * 240 + 3 + i] === strip[j * 24 + i];
  ok(same && eq([0xF0, 0x7D, 0x46, 0x4C, 79, ...scrEncode(strip, 3, 7, 24, 5)[0], 0xF7], FW), "the firmware's frame: Studio decodes it, and encodes the same bytes");
}
ok(scrDecode([0, 0, 2, 0, 0, 16, 0, 1, 0, 0, 0, 0, 0x40], a) === null && scrDecode([1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0], a) === null,
  "a box past the screen, or another type: refused");

/* ---- files */
const tone = (n, f) => Float32Array.from({ length: n }, (_, i) => 0.5 * Math.sin(i * f));
const w2 = wavFile([tone(100, 0.1), tone(100, 0.2)], 44100), dv = new DataView(w2.buffer);
ok(String.fromCharCode(...w2.slice(0, 4)) === "RIFF" && dv.getUint16(22, true) === 2 && dv.getUint32(24, true) === 44100 &&
   dv.getUint32(40, true) === 400 && w2.length === 444 && dv.getInt16(44 + 4, true) === Math.round(0.5 * Math.sin(0.1) * 32767), "WAV: stereo 16 bit, its header and first samples");
const w8 = wavFile(Array.from({ length: 8 }, (_, k) => tone(10, k / 10)), 48000), dv8 = new DataView(w8.buffer);
ok(dv8.getUint16(22, true) === 8 && dv8.getUint16(32, true) === 16 && dv8.getUint32(28, true) === 48000 * 16, "WAV: 8 channels (USB > STEMS), the block align and the byte rate");
ok(wavFile([new Float32Array([2, -2])], 44100).slice(44).every((v, i) => [0xFF, 0x7F, 0x00, 0x80][i] === v), "WAV: clipped at full scale");
const z = zipStored([{ name: "a/T1.wav", data: w2 }, { name: "a/MIX.wav", data: new Uint8Array([1, 2, 3]) }]), zv = new DataView(z.buffer);
const endAt = z.length - 22;
ok(zv.getUint32(0, true) === 0x04034B50 && zv.getUint32(14, true) === crc32(w2) && zv.getUint32(endAt, true) === 0x06054B50 &&
   zv.getUint16(endAt + 10, true) === 2 && crc32(new TextEncoder().encode("123456789")) === 0xCBF43926, "ZIP: stored entries, their CRC-32, the directory");
const mid = midiFile([{ t: 0, data: [0x90, 60, 100] }, { t: 500, data: [0x80, 60, 0] }, { t: 500, data: [0xC0, 5] }, { t: 250, data: [0xF8] }]);
const md = Array.from(mid);
ok(String.fromCharCode(...md.slice(0, 4)) === "MThd" && md[9] === 0 && (md[12] << 8 | md[13]) === 480 && String.fromCharCode(...md.slice(14, 18)) === "MTrk" &&
   eq(md.slice(22 + 7, 22 + 7 + 4), [0x00, 0x90, 60, 100]) && eq(md.slice(22 + 11, 22 + 11 + 5), [0x83, 0x60, 0x80, 60, 0]) &&
   eq(md.slice(22 + 16, 22 + 16 + 3), [0x00, 0xC0, 5]) && eq(md.slice(-4), [0x00, 0xFF, 0x2F, 0x00]), "MIDI file: format 0, 480 a quarter, 500 ms = 480 ticks, a clock left out");

/* ---- the page's words and views */
const page = readFileSync(join(HERE, "index.html"), "utf8"), js = readFileSync(join(HERE, "studio.js"), "utf8");
const tb = js.slice(js.indexOf("const TEXT = {"), js.indexOf("\n};", js.indexOf("const TEXT = {")) + 2);
const TEXT = vm.runInNewContext(tb.replace("const TEXT =", "(") + ")");
const used = new Set([...page.matchAll(/data-t(?:-label)?="(\w+)"/g), ...js.matchAll(/\bt\("(\w+)"\)/g)].map((m) => m[1]));
for (const m of js.matchAll(/t\((?:[^()]*?\? )?"(\w+)" : "(\w+)"\)/g)) { used.add(m[1]); used.add(m[2]); }
const miss = [...used].filter((k) => !(k in TEXT.es) || !(k in TEXT.en));
const odd = Object.keys(TEXT.es).filter((k) => !(k in TEXT.en)).concat(Object.keys(TEXT.en).filter((k) => !(k in TEXT.es)));
ok(!miss.length && !odd.length, `every word in Spanish and English (${used.size} used${miss.length ? "; missing " + miss : ""}${odd.length ? "; one language only " + odd : ""})`);
const views = [...page.matchAll(/<button data-go="(\w+)" data-t="v\w+">/g)].map((m) => m[1]);
ok(eq(views, ["home", "install", "editor", "screen", "emu", "rec", "proj", "docs", "rescue", "ports"]) && views.every((v) => page.includes(`id="v-${v}"`)) &&
   /const VIEWS = \["home", "install", "editor", "screen", "emu", "rec", "proj", "docs", "rescue", "ports"\]/.test(js), `ten views, a section each (${views.join(" ")})`);
/* other devices: every card's words, and its links (the emulator's panels exist in devices.html) */
const devices = readFileSync(join(HERE, "..", "emu", "devices.html"), "utf8");
const portKeys = [...js.matchAll(/\b(p[A-Z]\w+|st[A-Z]\w*|port[A-Z]\w+)"/g)].map((m) => m[1]).filter((k) => !/^(portlist|pic)$/.test(k));
ok(portKeys.length > 20 && portKeys.every((k) => k in TEXT.es && k in TEXT.en), `other devices: every word in both languages (${new Set(portKeys).size})`);
ok([...js.matchAll(/"#try:(\w+)"/g)].every((m) => devices.includes(`data-dev="${m[1]}"`)), "other devices: each emulator link opens a panel devices.html has");
ok(/new BroadcastChannel\("jiant-midi"\)/.test(js) && /type: "released", who: "studio"/.test(js) && /type: "studio", connected/.test(js),
  "install safety: on the jiant-midi channel, answers who and lets go (released)");
console.log(failed ? `STUDIO TESTS FAILED (${failed})` : "studio tests passed");
process.exit(failed ? 1 : 0);
