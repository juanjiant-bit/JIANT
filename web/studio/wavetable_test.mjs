/* the wavetable converter: a sine, a saw sweep and noise into 8 x 128; the SysEx packing round trip */
import { makeTable, pack, unpack, toI16, parseInfo, period, FRAMES, LEN, CHUNKS } from "./wavetable.js";
let failed = 0; const ok = (c, m) => { console.log(`wavetable: ${m}`.padEnd(86), c ? "ok" : "FAIL"); if (!c) failed++; };
const sr = 44100, n = sr;
const sine = Float32Array.from({ length: n }, (_, i) => Math.sin(2 * Math.PI * 220 * i / sr));
ok(Math.abs(period(sine.subarray(0, 4096), sr) - sr / 220) < 1.5, `a 220 Hz sine's period: ${period(sine.subarray(0, 4096), sr)} samples`);
const t = makeTable(sine, sr);
const ideal = Float32Array.from({ length: LEN }, (_, k) => Math.sin(2 * Math.PI * k / LEN));
const err = Math.max(...t.map((f) => Math.max(...f.map((v, k) => Math.abs(v - ideal[k])))));
ok(t.length === FRAMES && t.every((f) => f.length === LEN) && err < 0.12, `a sine: 8 frames of one sine cycle (max error ${err.toFixed(3)})`);
const saw = Float32Array.from({ length: n }, (_, i) => { const f = 110 * (1 + i / n); return 0; });
let ph = 0; for (let i = 0; i < n; i++) { ph += 110 * (1 + i / n) / sr; saw[i] = 2 * (ph - Math.floor(ph)) - 1; }
const ts = makeTable(saw, sr), jump = Math.max(...ts.map((f) => Math.abs(f[0] - f[LEN - 1])));
ok(ts.every((f) => Math.max(...f.map(Math.abs)) > 0.99), `a gliding saw: each frame at full level`);
let noise = Float32Array.from({ length: n }, () => Math.random() * 2 - 1);
ok(makeTable(noise, sr).length === FRAMES, "noise: still eight frames (no period: a fixed one)");
let silent = false; try { makeTable(new Float32Array(1000), sr); } catch (e) { silent = e.message === "silent"; }
ok(silent, "silence: refused");
const i16 = toI16(t[0]); let rt = [];
for (let c = 0; c < CHUNKS; c++) { const b = pack(i16, c); if (b.some((x) => x > 127) || b.length !== 96) rt = null; else rt.push(...unpack(b)); }
ok(rt && rt.every((v, k) => v === i16[k]), "SysEx packing: 4 chunks of 96 seven-bit bytes, back to the same int16");
const info = parseInfo([8, 8, 128, 0b0000101, 0, ...[..."PAD"].map((c) => c.charCodeAt(0)), 0, 0, ...[..."VOX"].map((c) => c.charCodeAt(0)), 0, 0, 0, 0, 0, 0]);
ok(info.n === 8 && info.used[0] && !info.used[1] && info.used[2] && info.names[0] === "PAD" && info.names[2] === "VOX", "WT_INFO: slots, used bits, names");
console.log(failed ? `WAVETABLE TESTS FAILED (${failed})` : "wavetable tests passed"); process.exit(failed ? 1 : 0);
