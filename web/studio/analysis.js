/* JIANT Studio's Analysis: the device's (or the emulator's) exact state and, from the emulator, how it sounds, in one
 * file to hand to someone (or to Claude): INFO, the tracks (TRACK 27; each one's parameters by TRACK_DUMP 29, named by
 * DESC with that track selected for a moment, the selection put back), the globals (DUMP 4, DESC scope 1), the steps
 * (TRACK_STEP 30), the song (SONG 33), the generator (jiantEmu.gen), and the audio (jiantEmu.tap: each track's level,
 * peak, crest, how long it sustains, eight octave bands). capture() -> an object; text() -> a summary to paste */
const v14 = (a, i) => (a[i] | (a[i + 1] << 7)) - 8192;
function str(a, i) { let s = ""; while (i < a.length && a[i]) s += String.fromCharCode(a[i++]); return [s, i + 1]; }
function info(a) {
  let i = 0, version; [version, i] = str(a, 0);
  const o = { version, nengines: a[i], pcount: a[i + 1], gcount: a[i + 2], nstep: a[i + 3], pe0: a[i + 4], engines: [] }; i += 5;
  for (let k = 0; k < o.nengines; k++) { let s; [s, i] = str(a, i); o.engines.push(s); }
  o.ntrk = i < a.length ? a[i] : 0;
  return o;
}
function desc(a) {
  const o = { fmt: a[2], min: v14(a, 3), max: v14(a, 5), def: v14(a, 7) };
  let i = 9; [o.label, i] = str(a, i); [o.unit, i] = str(a, i); o.names = [];
  if (o.fmt === 8) for (let k = 0; k <= o.max - o.min && k < 32 && i < a.length; k++) { let n; [n, i] = str(a, i); o.names.push(n); }
  return o;
}
const named = (d, v) => {
  if (!d) return { v };
  const o = { label: d.label || "?", v };
  if (d.names && d.names.length) o.name = d.names[v - d.min];
  if (d.unit) o.unit = d.unit;
  if (v !== d.def) o.def = d.names && d.names.length ? d.names[d.def - d.min] ?? d.def : d.def;
  o.range = [d.min, d.max];
  return o;
};
/* req(cmd, args, timeout, match) -> the reply's bytes; opts { steps, audio: tap fn or null, secs, gen }; say(text, frac) */
export async function capture(req, opts = {}, say = () => {}) {
  const out = { kind: "jiant-analysis", made: new Date().toISOString(), source: opts.source || "", note: "values are the device's raw ones; name = the enum's name; def = the default when the value differs" };
  say("INFO", 0);
  const I = info(await req(1, [], 900)); out.firmware = I.version; out.engines = I.engines;
  const T = await req(27, [], 900), sel = T[0], ntrk = T[1], tracks = [];
  for (let t = 0; t < ntrk; t++) { const b = 2 + t * 6; tracks.push({ track: t + 1, engine: I.engines[T[b]] || T[b], preset: T[b + 1] + 1, level: v14(T, b + 2), mute: !!T[b + 4] }); }
  out.selected = sel + 1;
  const common = [];                                             // the common parameters' names (any track)
  for (let id = 0; id < I.pe0; id++) { say(`DESC ${id + 1}/${I.pcount}`, id / I.pcount * .4); try { common[id] = desc(await req(5, [0, id], 800)); } catch (_) {} }
  try {
    for (let t = 0; t < ntrk; t++) {
      say(`T${t + 1}`, .4 + t / ntrk * .3);
      await req(27, [t], 900);                                   // this track for a moment: its engine's names
      const eng = [];
      for (let id = I.pe0; id < I.pcount; id++) { try { eng[id] = desc(await req(5, [0, id], 800)); } catch (_) {} }
      const D = await req(29, [t], 900), p = {};
      for (let id = 0; id < I.pcount; id++) { const v = v14(D, 3 + id * 2), d = id < I.pe0 ? common[id] : eng[id]; if (d && d.label && d.label !== "-") p[`${id}`] = named(d, v); }
      tracks[t].params = p;
    }
  } finally { try { await req(27, [sel], 900); } catch (_) {} }  // the selection back
  out.tracks = tracks;
  say("GLOBAL", .72);
  const G = await req(4, [], 1500), g = {};
  for (let id = 0; id < I.gcount; id++) { try { const d = desc(await req(5, [1, id], 800)); if (d.label && d.label !== "-") g[`${id}`] = named(d, v14(G, 2 + (I.pcount + id) * 2)); } catch (_) {} }
  out.globals = g;
  try { const S = await req(33, [0], 900); out.song = { count: S[2], playing: !!S[3], row: S[4], rows: Array.from({ length: S[2] }, (_, k) => ({ slot: "ABCD"[S[6 + k * 2]] || S[6 + k * 2], repeat: S[7 + k * 2] })) }; } catch (_) {}
  if (opts.steps) {
    for (let t = 0; t < ntrk; t++) {
      say(`STEPS T${t + 1}`, .78 + t / ntrk * .1);
      const st = [];
      for (let i = 0; i < Math.min(64, I.nstep); i++) {
        try { const a = await req(30, [t, i], 800, (r) => r[0] === t && r[1] === i); if (a[2]) st.push({ step: i + 1, n: a[2], notes: a.slice(3, 3 + a[2]), time: a[7], flags: a[8], vel: a[9] }); } catch (_) { break; }
      }
      tracks[t].steps = st;
    }
  }
  if (opts.gen) { try { const r = await opts.gen(); if (r) out.generator = r; } catch (_) {} }
  if (opts.audio) { say("AUDIO", .9); out.audio = await listen(opts.audio, opts.secs || 8, (f) => say(`AUDIO ${Math.round(f * 100)} %`, .9 + f * .1)); }
  say("", 1);
  return out;
}
/* the emulator's stems for a few seconds: per track and their sum, level / peak (dBFS), crest, sustain (the share of
   time above -40 dBFS), clipped samples, eight octave bands (dB, relative to the loudest) */
const BANDS = [63, 125, 250, 500, 1000, 2000, 4000, 8000];
function listen(tap, secs, prog) {
  return new Promise((res) => {
    const sr = 44100, n = 7, acc = Array.from({ length: n }, () => ({ sq: 0, peak: 0, cnt: 0, clip: 0, loud: 0, win: 0, wsq: 0, bands: BANDS.map((f) => ({ ...bp(f, sr), z1: 0, z2: 0, e: 0 })) }));
    let got = 0;
    const stop = tap((m) => {
      if (!m.planes) return;
      const L = m.planes[0].length;
      for (let k = 0; k < L; k++) {
        let mix = 0;
        for (let c = 0; c <= n; c++) {
          const x = c < 6 ? m.planes[c][k] : c === 6 ? mix + (m.planes[6] ? m.planes[6][k] : 0) : 0;
          if (c < 6) mix += x;
          if (c === 7) break;
          const A = acc[c], ax = Math.abs(x);
          A.sq += x * x; A.cnt++; if (ax > A.peak) A.peak = ax; if (ax > .99) A.clip++;
          A.wsq += x * x; if (++A.win === 441) { if (A.wsq / 441 > 1e-4) A.loud++; A.win = 0; A.wsq = 0; }
          for (const B of A.bands) { const y = B.b0 * x + B.z1; B.z1 = B.b1 * x - B.a1 * y + B.z2; B.z2 = B.b2 * x - B.a2 * y; B.e += y * y; }
        }
      }
      got += L; prog(Math.min(1, got / (sr * secs)));
      if (got >= sr * secs) { stop && stop(); res(summary()); }
    });
    if (!stop) res(null);
    const db = (v) => v > 1e-9 ? +(20 * Math.log10(v)).toFixed(1) : -120;
    function summary() {
      const names = ["T1", "T2", "T3", "T4", "T5", "T6", "MIX"];
      const o = { seconds: secs, rate: sr, bands_hz: BANDS, note: "the stems as USB STEMS sends them, before the master: a peak above 0 dBFS is headroom the master then limits; MIX = the six tracks plus the effects' return", tracks: {} };
      acc.forEach((A, c) => {
        const rms = Math.sqrt(A.sq / Math.max(1, A.cnt)), emax = Math.max(...A.bands.map((B) => B.e), 1e-12);
        o.tracks[names[c]] = rms < 1e-5 ? { silent: true } : { rms_dbfs: db(rms), peak_dbfs: db(A.peak), crest_db: +(db(A.peak) - db(rms)).toFixed(1),
          sustain_pct: Math.round(A.loud / Math.max(1, A.cnt / 441) * 100), clipped: A.clip, bands_db: A.bands.map((B) => +(10 * Math.log10(Math.max(B.e, 1e-12) / emax)).toFixed(1)) };
      });
      return o;
    }
  });
}
function bp(f, sr) {                                              // RBJ band-pass, an octave wide
  const w = 2 * Math.PI * f / sr, al = Math.sin(w) * Math.sinh(Math.LN2 / 2 * 1 * w / Math.sin(w)), a0 = 1 + al;
  return { b0: al / a0, b1: 0, b2: -al / a0, a1: -2 * Math.cos(w) / a0, a2: (1 - al) / a0 };
}
/* a summary to paste: the firmware, each track (engine, preset, level, the parameters away from their default), the
   globals away from theirs, the song, the generator, the audio */
export function text(o) {
  const L = [`JIANT ANALYSIS · ${o.firmware} · ${o.source} · ${o.made}`, ""];
  const pv = (p) => `${p.label}=${p.name ?? p.v}${p.unit ? p.unit : ""}${p.def != null ? ` (def ${p.def})` : ""}`;
  for (const t of o.tracks || []) {
    L.push(`T${t.track} ${t.engine} preset ${t.preset} level ${t.level}${t.mute ? " MUTED" : ""}${t.track === o.selected ? " [selected]" : ""}`);
    const ch = Object.values(t.params || {}).filter((p) => p.def != null);
    L.push(`  changed: ${ch.length ? ch.map(pv).join(", ") : "(all at default)"}`);
    if (t.steps) L.push(`  steps with notes: ${t.steps.length}${t.steps.length ? " · " + t.steps.slice(0, 16).map((s) => `${s.step}:${s.notes.join("/")}`).join(" ") : ""}`);
  }
  const gc = Object.values(o.globals || {}).filter((p) => p.def != null);
  L.push("", `GLOBAL changed: ${gc.length ? gc.map(pv).join(", ") : "(all at default)"}`);
  if (o.song) L.push(`SONG ${o.song.count} rows${o.song.playing ? ", playing" : ""}: ${o.song.rows.map((r) => `${r.slot}x${r.repeat}`).join(" ")}`);
  if (o.generator) L.push(`GENERATOR ${JSON.stringify(o.generator)}`);
  if (o.audio) {
    L.push("", `AUDIO ${o.audio.seconds} s (bands ${o.audio.bands_hz.join(" ")} Hz, dB from the loudest)`);
    for (const [k, a] of Object.entries(o.audio.tracks)) L.push(a.silent ? `  ${k} silent` : `  ${k} rms ${a.rms_dbfs} peak ${a.peak_dbfs} crest ${a.crest_db} sustain ${a.sustain_pct}% clipped ${a.clipped} bands ${a.bands_db.join(" ")}`);
  }
  return L.join("\n");
}
