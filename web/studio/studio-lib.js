// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// JIANT Studio's pure parts (no DOM; node tests: studio_test.mjs): the screen mirror's frames (as editor.html's scrDecode /
// scrEncode and the firmware's editor_screen.c), WAV and ZIP files, and a Standard MIDI File of what came in.

/* ---- the screen mirror (78 SCREEN, 79 SCREEN_DATA; web/EDITOR_PROTOCOL.md "Screen mirror"): a frame is the pixels of a
   box from pixel `start` on, row by row, as tokens: 0x00..0x3F the colour at that hash, 0x40..0x67 the colour before
   1..40 times more, 0x68..0x6F then a byte: 41..1064 times, 0x70..0x7F then two bytes: a colour in full (RGB565). Each
   frame starts with an empty hash table and the colour before 0. */
export const SCR = { W: 240, H: 240, CAP: 480 };
export const scrHash = (c) => ((c >> 11) * 3 + ((c >> 5) & 63) * 5 + (c & 31) * 7) & 63;
/* a = the frame's bytes after the command; fb: Uint16Array(240 * 240) of RGB565. -> the box drawn {x, y, w, h}, or null */
export function scrDecode(a, fb) {
  if (a.length < 12 || a[0] !== 0) return null;
  const x = a[1] | a[2] << 7, y = a[3] | a[4] << 7, w = a[5] | a[6] << 7, h = a[7] | a[8] << 7, end = w * h;
  let k = a[9] | a[10] << 7 | a[11] << 14;
  if (!w || !h || x + w > SCR.W || y + h > SCR.H) return null;
  const put = (c, r) => { for (; r > 0 && k < end; r--, k++) fb[(y + (k / w | 0)) * SCR.W + x + k % w] = c; };
  const tab = new Uint16Array(64);
  let prev = 0;
  for (let j = 12; j < a.length;) {
    const tk = a[j++];
    if (tk < 0x40) { prev = tab[tk]; put(prev, 1); }
    else if (tk < 0x68) put(prev, tk - 0x40 + 1);
    else if (tk < 0x70) put(prev, 41 + ((tk & 7) << 7 | (a[j++] | 0)));
    else { prev = (tk & 15) << 12 | (a[j] | 0) << 5 | (a[j + 1] | 0); j += 2; tab[scrHash(prev)] = prev; put(prev, 1); }
  }
  return { x, y, w, h };
}
/* the device's side (editor_screen.c scr_tap), for the simulated FM-1 and the tests: px RGB565, a row every `stride` */
export function scrEncode(px, x, y, w, h, stride = w, cap = SCR.CAP) {
  const frames = [];
  let out, tab, prev, run = 0, k = 0;
  const hdr = (start) => { out = [0, x & 127, x >> 7, y & 127, y >> 7, w & 127, w >> 7, h & 127, h >> 7,
    start & 127, (start >> 7) & 127, start >> 14]; tab = new Uint16Array(64); prev = 0; };
  const flushRun = () => {
    if (run > 40) { const r = run - 41; out.push(0x68 | r >> 7, r & 127); } else if (run) out.push(0x40 + run - 1);
    run = 0;
  };
  hdr(0);
  for (let j = 0; j < h; j++) for (let i = 0; i < w; i++, k++) {
    const c = px[j * stride + i] & 0xFFFF;
    if (out.length + 5 > 12 + cap) { flushRun(); frames.push(out); hdr(k); }
    if (c === prev) { if (++run === 1064) flushRun(); continue; }
    flushRun();
    const hh = scrHash(c);
    if (tab[hh] === c) out.push(hh); else { out.push(0x70 | c >> 12, (c >> 5) & 127, c & 31); tab[hh] = c; }
    prev = c;
  }
  flushRun();
  if (out.length > 12) frames.push(out);
  return frames;
}

/* ---- files ---- */
const u16 = (a, o, v) => { a[o] = v & 255; a[o + 1] = (v >>> 8) & 255; };
const u32 = (a, o, v) => { u16(a, o, v & 0xFFFF); u16(a, o + 2, v >>> 16); };
/* a 16-bit PCM WAV of Float32Array planes (1 mono, 2 stereo, ...), clipped at full scale */
export function wavFile(planes, rate) {
  const ch = planes.length, n = Math.min(...planes.map((p) => p.length)), bytes = n * ch * 2;
  const out = new Uint8Array(44 + bytes), dv = new DataView(out.buffer);
  const tag = (s, o) => { for (let i = 0; i < 4; i++) out[o + i] = s.charCodeAt(i); };
  tag("RIFF", 0); u32(out, 4, 36 + bytes); tag("WAVE", 8); tag("fmt ", 12); u32(out, 16, 16); u16(out, 20, 1); u16(out, 22, ch);
  u32(out, 24, rate); u32(out, 28, rate * ch * 2); u16(out, 32, ch * 2); u16(out, 34, 16); tag("data", 36); u32(out, 40, bytes);
  for (let i = 0, o = 44; i < n; i++) for (let c = 0; c < ch; c++, o += 2) dv.setInt16(o, Math.max(-32768, Math.min(32767, Math.round(planes[c][i] * 32767))), true);
  return out;
}
const CRC_T = (() => { const t = new Uint32Array(256); for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; t[n] = c >>> 0; } return t; })();
export function crc32(b) { let c = 0xFFFFFFFF; for (let i = 0; i < b.length; i++) c = CRC_T[(c ^ b[i]) & 255] ^ (c >>> 8); return (c ^ 0xFFFFFFFF) >>> 0; }
/* a ZIP of {name, data} entries, stored (no compression) */
export function zipStored(entries) {
  const enc = new TextEncoder(), parts = [], dir = [];
  let off = 0;
  for (const e of entries) {
    const name = enc.encode(e.name), crc = crc32(e.data), h = new Uint8Array(30 + name.length);
    u32(h, 0, 0x04034B50); u16(h, 4, 20); u32(h, 14, crc); u32(h, 18, e.data.length); u32(h, 22, e.data.length); u16(h, 26, name.length); h.set(name, 30);
    const c = new Uint8Array(46 + name.length);
    u32(c, 0, 0x02014B50); u16(c, 4, 20); u16(c, 6, 20); u32(c, 16, crc); u32(c, 20, e.data.length); u32(c, 24, e.data.length);
    u16(c, 28, name.length); u32(c, 42, off); c.set(name, 46);
    parts.push(h, e.data); dir.push(c); off += h.length + e.data.length;
  }
  const size = dir.reduce((s, c) => s + c.length, 0), end = new Uint8Array(22);
  u32(end, 0, 0x06054B50); u16(end, 8, entries.length); u16(end, 10, entries.length); u32(end, 12, size); u32(end, 16, off);
  const all = [...parts, ...dir, end], out = new Uint8Array(all.reduce((s, p) => s + p.length, 0));
  let o = 0; for (const p of all) { out.set(p, o); o += p.length; }
  return out;
}
/* a Standard MIDI File (format 0, 480 ticks a quarter at 120 BPM: a tick is 1/960 s) of [{t: ms, data: [status, d1, d2]}] */
export function midiFile(events, ppq = 480) {
  const trk = [0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20];   // tempo 500000 us a quarter
  const vlq = (v) => { const b = [v & 127]; while ((v >>= 7)) b.unshift((v & 127) | 128); return b; };
  const len = (st) => ((st & 0xF0) === 0xC0 || (st & 0xF0) === 0xD0 ? 1 : 2);
  let last = 0;
  for (const e of [...events].sort((x, y) => x.t - y.t)) {
    const st = e.data[0];
    if (st < 0x80 || st >= 0xF0) continue;
    const tick = Math.max(0, Math.round(e.t * ppq * 2 / 1000));
    trk.push(...vlq(tick - last), st, ...e.data.slice(1, 1 + len(st)).map((b) => b & 127));
    last = tick;
  }
  trk.push(0x00, 0xFF, 0x2F, 0x00);
  const out = new Uint8Array(14 + 8 + trk.length);
  const tag = (s, o) => { for (let i = 0; i < 4; i++) out[o + i] = s.charCodeAt(i); };
  const be32 = (o, v) => { out[o] = v >>> 24; out[o + 1] = (v >>> 16) & 255; out[o + 2] = (v >>> 8) & 255; out[o + 3] = v & 255; };
  tag("MThd", 0); be32(4, 6); out[9] = 0; out[11] = 1; out[12] = ppq >> 8; out[13] = ppq & 255;
  tag("MTrk", 14); be32(18, trk.length); out.set(trk, 22);
  return out;
}
