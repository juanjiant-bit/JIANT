/* JIANT BYTE's user wavetables: any sound -> a table of 8 frames x 128 int16 samples (WAVE's USER bank), and the
 * editor protocol's packing (1.5: WT_INFO 90, WT_WRITE 91, WT_COMMIT 92, WT_READ 93, WT_CHANGED 94 push).
 * Pure functions, no DOM: studio.js draws and sends; wavetable_test.mjs checks them. */
export const FRAMES = 8, LEN = 128, CHUNK = 32, CHUNKS = LEN / CHUNK;
export const WT = { INFO: 90, WRITE: 91, COMMIT: 92, READ: 93, CHANGED: 94 };

/* the period of x around its middle: normalized autocorrelation, lags of 30 Hz .. 2 kHz; the first lag close to the
   best one (so a note is not taken an octave down); 0 when nothing repeats (noise) */
export function period(x, sr) {
  const lo = Math.max(2, Math.floor(sr / 2000)), hi = Math.min(Math.floor(x.length / 2), Math.ceil(sr / 30));
  if (hi <= lo) return 0;
  const n = x.length - hi, r = new Float32Array(hi + 1);
  let e0 = 0; for (let i = 0; i < n; i++) e0 += x[i] * x[i];
  if (e0 < 1e-9) return 0;
  let best = -1, bl = 0;
  for (let l = lo; l <= hi; l++) {
    let s = 0, e1 = 0;
    for (let i = 0; i < n; i++) { s += x[i] * x[i + l]; e1 += x[i + l] * x[i + l]; }
    r[l] = s / Math.sqrt(e0 * e1 + 1e-12);
    if (r[l] > best) { best = r[l]; bl = l; }
  }
  if (best < 0.3) return 0;
  for (let l = lo + 1; l < hi; l++) if (r[l] >= 0.9 * best && r[l] >= r[l - 1] && r[l] >= r[l + 1]) return l;
  return bl;
}
/* one cycle of x from `at` (on a rising zero crossing near it), `p` samples long, resampled to LEN, DC out, the loop's
   seam closed with a ramp, peak 1 */
export function cycle(x, at, p) {
  let z = at;
  for (let d = 0; d < p; d++) { const i = at + d; if (i + 1 < x.length && x[i] <= 0 && x[i + 1] > 0) { z = i; break; } }
  const f = new Float32Array(LEN);
  for (let k = 0; k < LEN; k++) {
    const t = z + (k * p) / LEN, i = Math.floor(t), u = t - i;
    f[k] = (x[Math.min(i, x.length - 1)] || 0) * (1 - u) + (x[Math.min(i + 1, x.length - 1)] || 0) * u;
  }
  return tidy(f);
}
function tidy(f) {
  let m = 0; for (const v of f) m += v; m /= LEN;
  for (let k = 0; k < LEN; k++) f[k] -= m;
  const step = f[0] - (2 * f[LEN - 1] - f[LEN - 2]);              // the seam: where the last sample would go next
  for (let k = 0; k < LEN; k++) f[k] += (step * k) / (LEN - 1) / 2;
  let pk = 0; for (const v of f) pk = Math.max(pk, Math.abs(v));
  if (pk > 1e-6) for (let k = 0; k < LEN; k++) f[k] /= pk;
  return f;
}
/* the rotation of f that best follows prev (the sweep from frame to frame without a jump) */
function align(prev, f) {
  let best = -Infinity, br = 0;
  for (let r = 0; r < LEN; r++) { let s = 0; for (let k = 0; k < LEN; k++) s += prev[k] * f[(k + r) % LEN]; if (s > best) { best = s; br = r; } }
  return Float32Array.from({ length: LEN }, (_, k) => f[(k + br) % LEN]);
}
/* a sound (mono, sr) -> FRAMES frames (Float32Array, -1..1): its sounding part cut in eight, a cycle from each */
export function makeTable(x, sr) {
  let pk = 0; for (const v of x) pk = Math.max(pk, Math.abs(v));
  if (pk < 1e-4) throw new Error("silent");
  let a = 0, b = x.length - 1; const th = pk * 0.02;
  while (a < b && Math.abs(x[a]) < th) a++;
  while (b > a && Math.abs(x[b]) < th) b--;
  const len = b - a + 1, seg = len / FRAMES, frames = [];
  let lastP = 0;
  for (let k = 0; k < FRAMES; k++) {
    const c = Math.floor(a + (k + 0.5) * seg), w = Math.max(256, Math.min(8192, Math.floor(seg)));
    const s = Math.max(0, Math.min(x.length - w, c - (w >> 1)));
    let p = period(x.subarray(s, s + w), sr) || lastP || Math.max(16, Math.min(Math.floor(seg), Math.round(sr / 110)));
    lastP = p;
    let f = cycle(x, Math.max(0, c - (p >> 1)), p);
    if (frames.length) f = align(frames[frames.length - 1], f);
    frames.push(f);
  }
  return frames;
}
/* a frame of the table at a position 0..1 of the sweep (between two frames, crossfaded) */
export function morph(frames, pos) {
  const t = Math.max(0, Math.min(1, pos)) * (frames.length - 1), i = Math.min(frames.length - 2, Math.floor(t)), u = t - i;
  return Float32Array.from({ length: LEN }, (_, k) => frames[i][k] * (1 - u) + frames[i + 1][k] * u);
}
/* a preview: the sweep from frame 0 to 7 and back, played at hz, secs long (Float32Array at sr) */
export function render(frames, sr, hz = 110, secs = 3) {
  const n = Math.floor(sr * secs), out = new Float32Array(n);
  let ph = 0;
  for (let i = 0; i < n; i++) {
    const pos = 1 - Math.abs(1 - (2 * i) / n), t = pos * (FRAMES - 1), fi = Math.min(FRAMES - 2, Math.floor(t)), u = t - fi;
    const j = ph * LEN, k = Math.floor(j) % LEN, k2 = (k + 1) % LEN, v = j - Math.floor(j);
    const s0 = frames[fi][k] * (1 - v) + frames[fi][k2] * v, s1 = frames[fi + 1][k] * (1 - v) + frames[fi + 1][k2] * v;
    out[i] = 0.5 * (s0 * (1 - u) + s1 * u) * Math.min(1, i / 200, (n - i) / 200);
    ph += hz / sr; ph -= Math.floor(ph);
  }
  return out;
}
/* int16 as SysEx: 3 bytes of 7 bits (v = s + 32768) */
export const toI16 = (f) => Int16Array.from(f, (v) => Math.max(-32767, Math.min(32767, Math.round(v * 32000))));
export function pack(i16, chunk) {
  const out = [];
  for (let k = chunk * CHUNK; k < (chunk + 1) * CHUNK; k++) { const v = i16[k] + 32768; out.push(v & 127, (v >> 7) & 127, v >> 14); }
  return out;
}
export function unpack(bytes) {
  const out = [];
  for (let i = 0; i + 2 < bytes.length; i += 3) out.push((bytes[i] | (bytes[i + 1] << 7) | (bytes[i + 2] << 14)) - 32768);
  return out;
}
/* WT_INFO's answer: [nslots, frames, len, used bits (7 per byte), names\0...] */
export function parseInfo(a) {
  const n = a[0], nb = Math.ceil(n / 7), used = [], names = [];
  for (let s = 0; s < n; s++) used.push(!!(a[3 + Math.floor(s / 7)] & (1 << (s % 7))));
  let i = 3 + nb;
  for (let s = 0; s < n; s++) { let x = ""; while (i < a.length && a[i]) x += String.fromCharCode(a[i++]); i++; names.push(x); }
  return { n, frames: a[1], len: a[2], used, names };
}
/* a table as a WAV (int16, the 8 frames one after another: 1024 samples, the usual single-cycle-wavetable layout) */
export function wav(frames) {
  const d = new Int16Array(FRAMES * LEN); frames.forEach((f, i) => d.set(toI16(f), i * LEN));
  const b = new DataView(new ArrayBuffer(44 + d.length * 2)), w = (o, s) => [...s].forEach((c, i) => b.setUint8(o + i, c.charCodeAt(0)));
  w(0, "RIFF"); b.setUint32(4, 36 + d.length * 2, true); w(8, "WAVEfmt "); b.setUint32(16, 16, true); b.setUint16(20, 1, true); b.setUint16(22, 1, true);
  b.setUint32(24, 44100, true); b.setUint32(28, 88200, true); b.setUint16(32, 2, true); b.setUint16(34, 16, true); w(36, "data"); b.setUint32(40, d.length * 2, true);
  d.forEach((v, i) => b.setInt16(44 + i * 2, v, true));
  return new Blob([b.buffer], { type: "audio/wav" });
}
