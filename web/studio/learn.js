// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// JIANT Studio's "How it works": the animations. Each kind draws one idea (an engine, a sequencer, an effect, a
// modulator, a generator) on a canvas, as a loop in time: anim(kind)(c, t, W, H, params, palette). Drawn, not
// recorded: what moves is what the thing does (operators feeding a carrier, steps lighting, echoes fading, a formula
// of time painting its bytes). Player runs one on a canvas, pauses out of sight and with reduced motion shows a still.

const TAU = Math.PI * 2;
const lerp = (a, b, x) => a + (b - a) * x;
const fract = (x) => x - Math.floor(x);
const hash = (n) => fract(Math.sin(n * 127.1 + 311.7) * 43758.5453);
const tri = (x) => 1 - 4 * Math.abs(fract(x + .25) - .5);
const saw = (x) => 2 * fract(x + .5) - 1;
const sq = (x) => (fract(x) < .5 ? 1 : -1);

/* helpers: a trace of y = f(x) for x 0..1 across a box, a dot, a label */
function trace(c, f, x0, y0, w, h, col, lw = 2, n = 220) {
  c.beginPath();
  for (let i = 0; i <= n; i++) { const x = i / n, y = f(x); i ? c.lineTo(x0 + x * w, y0 + h / 2 - y * h / 2) : c.moveTo(x0 + x * w, y0 + h / 2 - y * h / 2); }
  c.strokeStyle = col; c.lineWidth = lw; c.lineJoin = "round"; c.stroke();
}
function dot(c, x, y, r, col, glow = 0) {
  c.beginPath(); c.arc(x, y, Math.max(.5, r), 0, TAU); c.fillStyle = col;
  if (glow) { c.shadowColor = col; c.shadowBlur = glow; }
  c.fill(); c.shadowBlur = 0;
}
function ring(c, x, y, r, col, lw = 1.5) { c.beginPath(); c.arc(x, y, r, 0, TAU); c.strokeStyle = col; c.lineWidth = lw; c.stroke(); }
function text(c, s, x, y, col, size = 12, align = "center") {
  c.fillStyle = col; c.font = `${size}px "IBM Plex Mono", ui-monospace, monospace`; c.textAlign = align; c.textBaseline = "middle"; c.fillText(s, x, y);
}
function box(c, x, y, w, h, col, fill) {
  c.beginPath(); c.roundRect ? c.roundRect(x, y, w, h, Math.min(6, h / 3)) : c.rect(x, y, w, h);
  if (fill) { c.fillStyle = fill; c.fill(); }
  if (col) { c.strokeStyle = col; c.lineWidth = 1.5; c.stroke(); }
}
const alpha = (col, a) => {                       // a CSS colour (#rgb, #rrggbb) at an alpha
  if (!col || col[0] !== "#") return col;
  const h = col.length === 4 ? col.slice(1).split("").map((x) => x + x).join("") : col.slice(1, 7);
  return `rgba(${parseInt(h.slice(0, 2), 16)},${parseInt(h.slice(2, 4), 16)},${parseInt(h.slice(4, 6), 16)},${a})`;
};

/* a step grid (16 by default) with a playhead; lit(i) says which steps play */
function grid(c, t, W, H, C, n, lit, rows = 1, label, pad = 18) {
  const gw = (W - pad - 18) / n, gh = Math.min(42, (H - 60) / rows), y0 = H / 2 - (gh * rows) / 2;
  const head = Math.floor(t * 4) % n;
  for (let r = 0; r < rows; r++) for (let i = 0; i < n; i++) {
    const on = lit(i, r), x = pad + i * gw + 2, y = y0 + r * gh + 2;
    box(c, x, y, gw - 4, gh - 4, alpha(C.fg, .18), on ? (i === head ? C.acc : alpha(C.acc, .45)) : i === head ? alpha(C.fg, .25) : null);
  }
  c.fillStyle = alpha(C.fg, .5); c.fillRect(pad + head * gw, y0 - 8, gw, 3);
  if (label) text(c, label, W / 2, y0 + rows * gh + 18, C.dim, 12);
  return head;
}

/* ------------------------------------------------------------------------------------------------- the kinds */
const K = {};
// ENGINES ----------------------------------------------------------------------------------------------------
K.fm = (c, t, W, H, p, C) => {            // operators (circles) orbit and feed the carrier: the wave bends with them
  const n = Math.max(2, Math.min(6, p.ops || 4)), cx = W * .28, cy = H / 2, R = Math.min(W, H) * .26;
  const idx = 1.2 + Math.sin(t * .7) * 1.1;
  for (let k = 0; k < n; k++) {
    const a = t * (.4 + k * .17) + k * TAU / n, x = cx + Math.cos(a) * R * (k ? .9 : 0), y = cy + Math.sin(a) * R * (k ? .9 : 0);
    if (k) { c.beginPath(); c.moveTo(x, y); c.lineTo(cx, cy); c.strokeStyle = alpha(C.a2, .35); c.lineWidth = 1; c.stroke(); }
    ring(c, x, y, k ? 12 + 4 * Math.sin(t * 2 + k) : 20, k ? C.a2 : C.acc, 2);
    text(c, k ? `OP${k + 1}` : "OP1", x, y, C.fg, 9);
  }
  const r = p.ratio || 2;
  trace(c, (x) => Math.sin(TAU * 3 * x + idx * Math.sin(TAU * 3 * r * x + t * 2)), W * .5, H * .2, W * .45, H * .6, C.acc, 2.5);
};
K.osc = (c, t, W, H, p, C) => {           // the shapes, morphing one into the next
  if ((p.shapes || []).includes("fold")) return K.west(c, t, W, H, p, C);
  const shapes = [Math.sin, (x) => saw(x / TAU), (x) => sq(x / TAU), (x) => tri(x / TAU)];
  const k = Math.floor(t / 2) % 4, m = Math.min(1, fract(t / 2) * 2), a = shapes[k], b = shapes[(k + 1) % 4];
  trace(c, (x) => lerp(a(TAU * 2 * x + t), b(TAU * 2 * x + t), m) * .85, 20, H * .15, W - 40, H * .7, C.acc, 3);
  text(c, ["SINE", "SAW", "SQUARE", "TRI"][(k + (m > .5 ? 1 : 0)) % 4], W / 2, H - 16, C.dim, 12);
};
K.phase = (c, t, W, H, p, C) => {         // phase distortion: a sine read at a bent speed
  const bend = .5 + .45 * Math.sin(t * .8);
  const ph = (x) => (x < bend ? x / bend * .5 : .5 + (x - bend) / (1 - bend) * .5);
  trace(c, (x) => ph(fract(x * 2)) * 2 - 1, 20, H * .1, W * .4, H * .5, alpha(C.a2, .8), 2);
  trace(c, (x) => Math.cos(TAU * ph(fract(x * 2 + t * .2))), W * .5, H * .15, W * .45, H * .7, C.acc, 2.5);
  text(c, "phase", W * .2, H * .72, C.dim, 11); text(c, "out", W * .72, H * .92, C.dim, 11);
};
K.lofi = (c, t, W, H, p, C) => {          // fewer bits, fewer samples: the sine as stairs
  const steps = 3 + Math.floor((Math.sin(t * .6) * .5 + .5) * 12), rate = 18 + Math.floor((Math.cos(t * .4) * .5 + .5) * 60);
  trace(c, (x) => Math.sin(TAU * 2 * x + t), 20, H * .15, W - 40, H * .7, alpha(C.fg, .2), 1.5);
  trace(c, (x) => Math.round(Math.sin(TAU * 2 * (Math.floor(x * rate) / rate) + t) * steps) / steps, 20, H * .15, W - 40, H * .7, C.acc, 2.5, 600);
  text(c, `${steps * 2 + 1} levels · ${rate} samples`, W / 2, H - 14, C.dim, 11);
};
K.filter = (c, t, W, H, p, C) => {        // a spectrum, the cutoff sweeping with its resonant peak
  const cut = .2 + .6 * (Math.sin(t * .7) * .5 + .5), res = p.res ?? .7, n = 48;
  for (let i = 0; i < n; i++) {
    const x = i / n, g = x < cut ? 1 : Math.exp(-(x - cut) * 14), pk = res * Math.exp(-((x - cut) ** 2) * 900) * 1.4;
    const h = (H - 50) * Math.min(1.3, (g + pk) * (.45 + .3 * hash(i + Math.floor(t * 8)))) ;
    c.fillStyle = alpha(C.acc, .35 + .65 * Math.min(1, g + pk)); c.fillRect(20 + x * (W - 40), H - 24 - h, (W - 40) / n - 2, h);
  }
  trace(c, (x) => -1 + 2 * Math.min(1, (x < cut ? 1 : Math.exp(-(x - cut) * 14)) * .8 + res * Math.exp(-((x - cut) ** 2) * 900)), 20, 20, W - 40, H - 44, C.fg, 2);
  text(c, "CUTOFF", 20 + cut * (W - 40), 12, C.fg, 11);
};
K.string = (c, t, W, H, p, C) => {        // a plucked string: a kink that travels and dies away
  const tt = fract(t / 2.5) * 2.5, d = Math.exp(-tt * 1.4);
  trace(c, (x) => d * (Math.sin(Math.PI * x) * Math.cos(t * 9) * .7 + Math.sin(3 * Math.PI * x) * Math.cos(t * 23) * .25), 30, H * .2, W - 60, H * .6, C.acc, 3);
  dot(c, 30, H / 2, 4, C.fg); dot(c, W - 30, H / 2, 4, C.fg);
  if (tt < .2) text(c, "PLUCK", W * .3, H * .18, C.fg, 12);
};
K.additive = (c, t, W, H, p, C) => {      // partials rising and falling: the sum on top
  const n = p.partials || 12, bw = (W - 40) / n;
  const amp = (k) => (Math.sin(t * (.6 + k * .13) + k) * .5 + .5) / (1 + k * .35);
  for (let k = 0; k < n; k++) { const h = amp(k) * (H * .45); c.fillStyle = alpha(C.acc, .3 + .7 * amp(k)); c.fillRect(20 + k * bw + 2, H - 20 - h, bw - 4, h); }
  trace(c, (x) => { let s = 0; for (let k = 0; k < n; k++) s += amp(k) * Math.sin(TAU * (k + 1) * x); return s * .45; }, 20, 10, W - 40, H * .4, C.fg, 2);
};
K.noise = (c, t, W, H, p, C) => {
  for (let i = 0; i < 380; i++) {
    const x = hash(i * 3.1 + Math.floor(t * 20)) * W, y = H / 2 + (hash(i * 7.7 + Math.floor(t * 20)) - .5) * H * .8 * (.5 + .5 * Math.sin(t + x / 90));
    c.fillStyle = alpha(i % 5 ? C.fg : C.acc, .55); c.fillRect(x, y, 2, 2);
  }
};
K.grain = (c, t, W, H, p, C) => {         // grains: small windows of a sound, scattered in time and pitch
  trace(c, (x) => Math.sin(TAU * 3 * x) * Math.sin(Math.PI * x) * .8, 20, 16, W - 40, H * .35, alpha(C.fg, .3), 1.5);
  for (let i = 0; i < 26; i++) {
    const age = fract(t * .8 + hash(i)), x = 20 + hash(i * 9 + Math.floor(t * .8 + hash(i))) * (W - 60), y = H * .55 + (hash(i * 4.4) - .5) * H * .5;
    const r = 6 + 16 * Math.sin(Math.PI * age);
    c.beginPath(); c.ellipse(x, y, r * 1.6, r * .5, 0, 0, TAU); c.fillStyle = alpha(i % 3 ? C.acc : C.a2, .7 * Math.sin(Math.PI * age)); c.fill();
  }
};
K.voice = (c, t, W, H, p, C) => {         // a mouth: the formants move, vowel to vowel
  const v = ["A", "E", "I", "O", "U"], k = Math.floor(t / 1.2) % 5, open = .3 + .7 * Math.abs(Math.sin(t * 2.6));
  c.beginPath(); c.ellipse(W * .3, H / 2, 56, 10 + 40 * open * (k === 2 ? .35 : k === 4 ? .6 : 1), 0, 0, TAU);
  c.strokeStyle = C.acc; c.lineWidth = 3; c.stroke();
  text(c, v[k], W * .3, H / 2, C.fg, 22);
  const F = [[.15, .45], [.12, .7], [.08, .85], [.14, .3], [.09, .25]][k];
  trace(c, (x) => (Math.exp(-((x - F[0]) ** 2) * 300) + .8 * Math.exp(-((x - F[1]) ** 2) * 300)) * 1.6 - .8, W * .55, H * .2, W * .4, H * .6, C.a2, 2.5);
  text(c, "F1      F2", W * .75, H * .9, C.dim, 11);
};
K.creature = (c, t, W, H, p, C) => {      // a creature breathing: what the engine draws on the screen, alive
  const cx = W / 2, cy = H / 2, R = Math.min(W, H) * .26, legs = p.legs || 7;
  c.beginPath();
  for (let i = 0; i <= 120; i++) {
    const a = i / 120 * TAU, r = R * (1 + .18 * Math.sin(legs * a + t * 2) + .07 * Math.sin(3 * a - t * 3)) * (1 + .05 * Math.sin(t * 1.7));
    i ? c.lineTo(cx + Math.cos(a) * r, cy + Math.sin(a) * r) : c.moveTo(cx + Math.cos(a) * r, cy + Math.sin(a) * r);
  }
  c.closePath(); c.fillStyle = alpha(C.acc, .12); c.fill(); c.strokeStyle = C.acc; c.lineWidth = 2.5; c.stroke();
  ring(c, cx, cy, R * .55, alpha(C.a2, .6), 2);
  const blink = fract(t / 3) < .06 ? .2 : 1;
  c.fillStyle = C.fg; c.fillRect(cx - 12, cy - 4, 5, 8 * blink); c.fillRect(cx + 7, cy - 4, 5, 8 * blink);
};
K.drum = (c, t, W, H, p, C) => {          // a kit: each voice its row, each hit a flash
  const names = ["KICK", "SNARE", "HAT", "PERC"], pat = [0x1111, 0x0808, 0xaaaa, 0x2244];
  const head = grid(c, t * 1.2, W, H, C, 16, (i, r) => (pat[r] >> (15 - i)) & 1, 4, null, 62);
  const gh = Math.min(42, (H - 60) / 4);
  names.forEach((n, r) => { const on = (pat[r] >> (15 - head)) & 1; text(c, n, 12, H / 2 - gh * 2 + r * gh + gh / 2, on ? C.acc : C.dim, 11, "left"); });
};
K.wheel = (c, t, W, H, p, C) => {         // a wheel of steps turning: each spoke a sound
  const cx = W / 2, cy = H / 2, R = Math.min(W, H) * .34, n = 8;
  ring(c, cx, cy, R, alpha(C.fg, .25), 2);
  for (let k = 0; k < n; k++) {
    const a = t * .8 + k * TAU / n, on = k === (Math.floor(t * 2) % n);
    c.beginPath(); c.moveTo(cx, cy); c.lineTo(cx + Math.cos(a) * R, cy + Math.sin(a) * R); c.strokeStyle = alpha(C.acc, on ? 1 : .35); c.lineWidth = on ? 3 : 1.5; c.stroke();
    dot(c, cx + Math.cos(a) * R, cy + Math.sin(a) * R, on ? 9 : 5, on ? C.acc : C.a2, on ? 12 : 0);
  }
};
K.west = (c, t, W, H, p, C) => {          // west coast: a wavefolder folding a sine, a low-pass gate opening on each hit
  const fold = 1 + 3 * (Math.sin(t * .6) * .5 + .5), gate = Math.exp(-fract(t * 1.2) * 4);
  trace(c, (x) => Math.sin(TAU * 2 * x) * .8, 20, H * .1, W * .42, H * .4, alpha(C.fg, .35), 1.5);
  trace(c, (x) => Math.sin(Math.sin(TAU * 2 * x) * fold * 1.6) * .8 * gate, W * .52, H * .2, W * .45, H * .6, C.acc, 2.5);
  text(c, `FOLD ×${fold.toFixed(1)}`, W * .2, H * .62, C.dim, 11); text(c, "LPG", W * .75, H * .92, C.dim, 11);
};
K.input = (c, t, W, H, p, C) => {         // the outside world in: a jack, the signal through the track
  box(c, 20, H / 2 - 20, 60, 40, C.fg); text(c, "IN", 50, H / 2, C.fg, 12);
  trace(c, (x) => (Math.sin(TAU * 4 * x + t * 3) * .5 + (hash(Math.floor(x * 90 + t * 30)) - .5) * .5), 90, H * .25, W - 110, H * .5, C.acc, 2);
};
K.math = (c, t, W, H, p, C) => {          // a formula of time: each column a moment, each pixel a bit of its byte
  const f = (tt) => (tt * ((tt >> 5) | (tt >> 8))) & 255, cols = 96, rows = 8, cw = (W - 40) / cols, ch = (H - 70) / rows, t0 = Math.floor(t * 900);
  for (let x = 0; x < cols; x++) { const v = f(t0 + x * 37); for (let b = 0; b < rows; b++) if ((v >> b) & 1) { c.fillStyle = alpha(b > 4 ? C.acc : C.a2, .4 + b * .07); c.fillRect(20 + x * cw, 20 + (rows - 1 - b) * ch, cw - 1, ch - 2); } }
  text(c, p.formula || "t*(t>>5|t>>8)", W / 2, H - 22, C.fg, 13);
};
K.bytebeat = K.math;
K.floatbeat = (c, t, W, H, p, C) => {     // floatbeat: the same idea in real numbers, a smooth function of time
  trace(c, (x) => Math.sin(TAU * 5 * x + t * 2) * Math.sin(TAU * .5 * x + Math.sin(t) * 3) * .9, 20, 16, W - 40, H - 60, C.acc, 2.5, 400);
  text(c, p.formula || "sin(t·ω)·sin(t·ω/10)", W / 2, H - 18, C.fg, 12);
};
K.wave = (c, t, W, H, p, C) => {          // a wavetable: a stack of waves, the reading position moving through it
  const n = 9, pos = (Math.sin(t * .7) * .5 + .5) * (n - 1);
  for (let k = n - 1; k >= 0; k--) {
    const on = Math.abs(k - pos) < .5, y = 20 + k * (H - 60) / n;
    trace(c, (x) => Math.sin(TAU * x + Math.sin(TAU * x * (1 + k * .4)) * k * .3) * .8, 30 + k * 6, y, W - 90, 34, on ? C.acc : alpha(C.fg, .25), on ? 2.5 : 1);
  }
};
K.drone = (c, t, W, H, p, C) => {         // a drone and its slow modulators: layers drifting
  for (let k = 0; k < 5; k++) trace(c, (x) => Math.sin(TAU * (1 + k * .5) * x + t * (.2 + k * .07) + k) * (.3 + .15 * Math.sin(t * .3 + k)), 20, H * .2, W - 40, H * .6, alpha(k ? C.a2 : C.acc, k ? .45 : 1), k ? 1.5 : 3);
};
// SEQUENCERS -------------------------------------------------------------------------------------------------
K.seq = (c, t, W, H, p, C) => { const n = p.steps || 16; grid(c, t, W, H, C, n, (i) => [0, 3, 6, 8, 10, 12, 14].includes(i % 16)); };
K.euclid = (c, t, W, H, p, C) => {        // k hits spread as evenly as can be over n steps, on a circle, rotating
  const n = p.steps || 16, k = p.hits || 5, cx = W / 2, cy = H / 2, R = Math.min(W, H) * .33, rot = Math.floor(t / 3) % n, head = Math.floor(t * 4) % n;
  ring(c, cx, cy, R, alpha(C.fg, .2), 1.5);
  const hit = (i) => Math.floor(((i + rot) % n) * k / n) !== Math.floor((((i + rot) % n) - 1 + n) % n * k / n) || (i + rot) % n === 0;
  c.beginPath(); let first = true;
  for (let i = 0; i < n; i++) if (hit(i)) { const a = i / n * TAU - Math.PI / 2, x = cx + Math.cos(a) * R, y = cy + Math.sin(a) * R; first ? c.moveTo(x, y) : c.lineTo(x, y); first = false; }
  c.closePath(); c.strokeStyle = alpha(C.acc, .5); c.lineWidth = 1.5; c.stroke();
  for (let i = 0; i < n; i++) { const a = i / n * TAU - Math.PI / 2, on = hit(i); dot(c, cx + Math.cos(a) * R, cy + Math.sin(a) * R, i === head ? 9 : on ? 7 : 3, i === head ? C.fg : on ? C.acc : C.dim, i === head && on ? 14 : 0); }
  text(c, `${k} / ${n}`, cx, cy, C.fg, 16);
};
K.modes = (c, t, W, H, p, C) => {         // the same pattern played forward, backward, ping-pong, random
  const modes = ["FWD", "REV", "PING", "RND"], m = Math.floor(t / 3) % 4, n = 16, s = Math.floor(t * 4);
  const head = m === 0 ? s % n : m === 1 ? n - 1 - (s % n) : m === 2 ? (s % (2 * n - 2) < n ? s % (2 * n - 2) : 2 * n - 2 - (s % (2 * n - 2))) : Math.floor(hash(s) * n);
  const pad = 18, gw = (W - 36) / n;
  for (let i = 0; i < n; i++) box(c, pad + i * gw + 2, H / 2 - 20, gw - 4, 40, alpha(C.fg, .18), i === head ? C.acc : null);
  text(c, modes[m], W / 2, H / 2 + 46, C.fg, 14);
};
K.mutate = (c, t, W, H, p, C) => {        // each pass some steps change: the pattern evolves, keeping its shape
  const gen = Math.floor(t / 2);
  grid(c, t, W, H, C, 16, (i) => hash(i * 3 + Math.floor(gen / 3)) < .35 !== (hash(i * 11 + gen) < .12), 1, `gen ${gen % 100}`);
};
K.locks = (c, t, W, H, p, C) => {         // a step carries its own value of a knob: the knob jumps there and back
  const n = 16, head = Math.floor(t * 4) % n, locked = { 2: .9, 6: .2, 11: .7, 14: .4 };
  grid(c, t, W, H * .7, C, n, (i) => i % 2 === 0 || i in locked);
  for (const i in locked) text(c, "◆", 18 + (+i + .5) * (W - 36) / n, H * .35 - 34, C.a2, 12);
  const v = locked[head] ?? .5, cx = W / 2, cy = H * .8;
  ring(c, cx, cy, 22, alpha(C.fg, .3), 3);
  c.beginPath(); c.arc(cx, cy, 22, Math.PI * .75, Math.PI * .75 + v * Math.PI * 1.5); c.strokeStyle = head in locked ? C.a2 : C.acc; c.lineWidth = 4; c.stroke();
};
K.stepfx = K.locks;
K.chord = (c, t, W, H, p, C) => {         // one key, a chord: the notes stack over it
  const n = p.notes || 4, chords = [[0, 4, 7, 11], [0, 3, 7, 10], [0, 5, 7, 12], [0, 4, 7, 14]], ch = chords[Math.floor(t / 1.6) % 4].slice(0, n);
  const kw = (W - 40) / 24;
  for (let k = 0; k < 24; k++) { const on = ch.includes(k); box(c, 20 + k * kw + 1, H * .55, kw - 2, H * .35, alpha(C.fg, .25), on ? C.acc : [1, 3, 6, 8, 10].includes(k % 12) ? alpha(C.fg, .12) : null); }
  ch.forEach((k, i) => dot(c, 20 + (k + .5) * kw, H * .45 - i * 18, 7, i ? C.a2 : C.acc, 8));
};
K.arp = (c, t, W, H, p, C) => {           // the held notes, played one by one in a pattern
  const notes = [0, 4, 7, 12], pat = p.pattern || "updown", s = Math.floor(t * 5);
  const seq = pat === "down" ? [3, 2, 1, 0] : pat === "up" ? [0, 1, 2, 3] : pat === "random" || pat === "formula" ? [0, 2, 1, 3, 3, 0, 2] : [0, 1, 2, 3, 2, 1];
  const cur = seq[s % seq.length];
  for (let i = 0; i < 24; i++) { const k = seq[(s - 23 + i + 24 * seq.length) % seq.length]; dot(c, 20 + i * (W - 40) / 23, H * .8 - notes[k] * (H * .05), i === 23 ? 8 : 4, i === 23 ? C.acc : alpha(C.a2, .3 + i / 30)); }
  text(c, `${pat.toUpperCase()} · ${["C", "E", "G", "C"][cur]}`, W / 2, 18, C.fg, 12);
};
K.scale = (c, t, W, H, p, C) => {         // only the notes of the scale: the keys outside fall to the nearest inside
  const scales = [[0, 2, 4, 5, 7, 9, 11], [0, 2, 3, 5, 7, 8, 10], [0, 3, 5, 7, 10], [0, 2, 4, 7, 9]], names = ["MAJOR", "MINOR", "MINOR PENT", "MAJOR PENT"];
  const k = Math.floor(t / 2.5) % 4, sc = scales[k], kw = (W - 40) / 12;
  for (let i = 0; i < 12; i++) box(c, 20 + i * kw + 2, H * .3, kw - 4, H * .4, alpha(C.fg, .25), sc.includes(i) ? alpha(C.acc, .8) : null);
  text(c, names[k], W / 2, H * .82, C.fg, 13);
};
K.jam = (c, t, W, H, p, C) => {           // each track picks a clip, A–D: they change together, at the bar
  const rows = 6, cols = 4, bar = Math.floor(t / 2), cw = (W - 60) / cols, ch = (H - 30) / rows;
  for (let r = 0; r < rows; r++) for (let k = 0; k < cols; k++) {
    const on = Math.floor(hash(r * 7 + Math.floor(bar / 2)) * 4) === k;
    box(c, 40 + k * cw + 3, 15 + r * ch + 3, cw - 6, ch - 6, alpha(C.fg, .2), on ? alpha(C.acc, .5 + .5 * (1 - fract(t / 2))) : null);
  }
  ["A", "B", "C", "D"].forEach((s, k) => text(c, s, 40 + (k + .5) * cw, 8, C.dim, 10));
};
// EFFECTS ----------------------------------------------------------------------------------------------------
K.delay = (c, t, W, H, p, C) => {         // a hit and its echoes, each fainter, at the time apart
  const tt = fract(t / 3) * 3, gap = .38;
  for (let k = 0; k < 7; k++) { const at = k * gap; if (tt < at) break; const a = Math.pow(.62, k), x = 30 + at / 3 * (W - 60) * 1.2; c.fillStyle = alpha(k ? C.a2 : C.acc, a); c.fillRect(x, H / 2 - 60 * a, 8, 120 * a); }
  text(c, "TIME · FEEDBACK", W / 2, H - 14, C.dim, 11);
};
K.reverb = (c, t, W, H, p, C) => {        // a room answering: rings spreading, many reflections smearing into a tail
  const cx = W / 2, cy = H / 2;
  for (let k = 0; k < 9; k++) { const a = fract(t * .35 + k / 9), r = a * Math.max(W, H) * .6; ring(c, cx, cy, r, alpha(k % 2 ? C.a2 : C.acc, (1 - a) * .8), 2); }
  dot(c, cx, cy, 6, C.fg, 10);
};
K.chorus = (c, t, W, H, p, C) => {        // copies a little detuned and late, drifting: the sound widens
  for (let k = 0; k < 3; k++) trace(c, (x) => Math.sin(TAU * 3 * x + t * 2 + Math.sin(t * .9 + k * 2) * .6) * .7, 20, H * .15, W - 40, H * .7, alpha(k ? C.a2 : C.acc, k ? .5 : 1), k ? 2 : 3);
};
K.slicer = (c, t, W, H, p, C) => {        // the sound cut into rhythmic slices
  const gate = (x) => (fract(x * 8 - t * 2) < (hash(Math.floor(x * 8 - t * 2)) > .4 ? .7 : .25) ? 1 : 0);
  trace(c, (x) => Math.sin(TAU * 12 * x) * .8 * gate(x), 20, H * .15, W - 40, H * .7, C.acc, 2, 500);
};
K.punch = (c, t, W, H, p, C) => {         // hold FX and a key: an effect only while held
  const held = fract(t / 2) < .5;
  for (let i = 0; i < 8; i++) box(c, 20 + i * (W - 40) / 8 + 3, H * .6, (W - 40) / 8 - 6, H * .3, alpha(C.fg, .25), held && i === 3 ? C.acc : null);
  text(c, held ? "FX + KEY: REPEAT ▮▮▮▮" : "—", W / 2, H * .3, held ? C.acc : C.dim, 14);
};
K.master = (c, t, W, H, p, C) => {        // the mix's meters and the limiter's ceiling
  for (let ch = 0; ch < 2; ch++) {
    const v = Math.min(1, .55 + .45 * Math.abs(Math.sin(t * 3.3 + ch)) * hash(Math.floor(t * 12) + ch)), w = (W - 80) * v;
    box(c, 40, H * .32 + ch * 36, W - 80, 24, alpha(C.fg, .2)); c.fillStyle = v > .9 ? C.acc : C.a2; c.fillRect(42, H * .32 + ch * 36 + 2, w - 4, 20);
  }
  c.fillStyle = C.fg; c.fillRect(40 + (W - 80) * .92, H * .28, 2, 84); text(c, "CEILING", 40 + (W - 80) * .92, H * .22, C.dim, 10);
};
K.djfilter = (c, t, W, H, p, C) => {      // one knob: left low-pass, right high-pass, the middle clean
  const v = Math.sin(t * .6);
  trace(c, (x) => (v < 0 ? (x < .5 + v * .45 ? .8 : -.8) : (x > v * .45 ? .8 : -.8)), 20, H * .2, W - 40, H * .5, C.acc, 3);
  text(c, v < -.1 ? "LOW-PASS" : v > .1 ? "HIGH-PASS" : "CLEAN", W / 2, H * .85, C.fg, 13);
};
K.buses = (c, t, W, H, p, C) => {         // six tracks sending to the shared effects
  const fx = ["DELAY", "REVERB", "CHORUS"];
  for (let i = 0; i < 6; i++) { const y = 20 + i * (H - 40) / 6 + 10; box(c, 16, y - 9, 40, 18, alpha(C.fg, .3)); text(c, `T${i + 1}`, 36, y, C.fg, 10);
    fx.forEach((f, k) => { const s = Math.sin(t + i * 1.7 + k) * .5 + .5; c.beginPath(); c.moveTo(56, y); c.lineTo(W - 110, H * (.25 + k * .25)); c.strokeStyle = alpha(C.acc, s * .7); c.lineWidth = 1 + s * 2; c.stroke(); }); }
  fx.forEach((f, k) => { box(c, W - 108, H * (.25 + k * .25) - 12, 92, 24, C.a2); text(c, f, W - 62, H * (.25 + k * .25), C.fg, 10); });
};
// MODULATORS -------------------------------------------------------------------------------------------------
K.lfo = (c, t, W, H, p, C) => {           // a slow wave (its shape), and the knob it moves
  const sh = p.shape || "sine", f = sh === "step" ? (x) => hash(Math.floor(x * 4)) * 2 - 1 : sh === "tri" ? tri : sh === "saw" ? saw : sh === "square" ? sq : (x) => Math.sin(TAU * x);
  trace(c, (x) => f(x * 2 + t * .4) * .8, 20, H * .15, W * .62, H * .7, C.acc, 2.5);
  const v = f(t * .4) * .5 + .5, cx = W * .83, cy = H / 2;
  ring(c, cx, cy, 30, alpha(C.fg, .3), 3);
  c.beginPath(); c.arc(cx, cy, 30, Math.PI * .75, Math.PI * .75 + v * Math.PI * 1.5); c.strokeStyle = C.a2; c.lineWidth = 5; c.stroke();
  dot(c, 20, H / 2 - f(t * .4) * .8 * H * .35, 6, C.fg, 10);
};
K.env = (c, t, W, H, p, C) => {           // attack, decay, sustain, release, and a dot riding it
  const pts = [[0, 0], [.15, 1], [.35, .6], [.7, .6], [1, 0]], x0 = 30, y0 = H - 30, w = W - 60, h = H - 70;
  c.beginPath(); pts.forEach(([x, y], i) => (i ? c.lineTo(x0 + x * w, y0 - y * h) : c.moveTo(x0 + x * w, y0 - y * h))); c.strokeStyle = C.acc; c.lineWidth = 3; c.stroke();
  const u = fract(t / 3); let i = 0; while (i < 3 && u > pts[i + 1][0]) i++;
  const a = pts[i], b = pts[i + 1], m = (u - a[0]) / (b[0] - a[0]);
  dot(c, x0 + u * w, y0 - lerp(a[1], b[1], m) * h, 7, C.fg, 12);
  ["A", "D", "S", "R"].forEach((s, k) => text(c, s, x0 + (pts[k][0] + pts[k + 1][0]) / 2 * w, y0 + 16, C.dim, 12));
};
K.matrix = (c, t, W, H, p, C) => {        // sources on the left, destinations on the right, routes with amounts
  const src = ["LFO", "ENV", "VEL", "M1"], dst = ["PITCH", "CUT", "AMP", "PAN"], routes = [[0, 1], [1, 2], [2, 1], [3, 3], [0, 3]];
  src.forEach((s, i) => { box(c, 20, 24 + i * (H - 48) / 4, 70, 26, C.a2); text(c, s, 55, 37 + i * (H - 48) / 4, C.fg, 11); });
  dst.forEach((s, i) => { box(c, W - 90, 24 + i * (H - 48) / 4, 70, 26, C.acc); text(c, s, W - 55, 37 + i * (H - 48) / 4, C.fg, 11); });
  routes.forEach(([a, b], k) => { const amt = Math.sin(t * 1.3 + k) * .5 + .5, y1 = 37 + a * (H - 48) / 4, y2 = 37 + b * (H - 48) / 4;
    c.beginPath(); c.moveTo(90, y1); c.bezierCurveTo(W / 2, y1, W / 2, y2, W - 90, y2); c.strokeStyle = alpha(C.fg, .2 + amt * .7); c.lineWidth = 1 + amt * 3; c.stroke();
    const u = fract(t * .7 + k / 5); dot(c, lerp(90, W - 90, u), lerp(y1, y2, u * u * (3 - 2 * u)), 3, C.fg); });
};
K.macro = (c, t, W, H, p, C) => {         // one macro knob turning several things at once
  const v = Math.sin(t * .8) * .5 + .5, cx = W * .2, cy = H / 2;
  ring(c, cx, cy, 34, alpha(C.fg, .3), 4); c.beginPath(); c.arc(cx, cy, 34, Math.PI * .75, Math.PI * .75 + v * Math.PI * 1.5); c.strokeStyle = C.acc; c.lineWidth = 6; c.stroke(); text(c, "M1", cx, cy, C.fg, 14);
  for (let k = 0; k < 4; k++) { const x = W * (.45 + k * .14), vv = [v, 1 - v, v * v, Math.sqrt(v)][k];
    c.beginPath(); c.moveTo(cx + 36, cy); c.lineTo(x - 20, cy); c.strokeStyle = alpha(C.fg, .15); c.stroke();
    ring(c, x, cy, 18, alpha(C.fg, .3), 3); c.beginPath(); c.arc(x, cy, 18, Math.PI * .75, Math.PI * .75 + vv * Math.PI * 1.5); c.strokeStyle = C.a2; c.lineWidth = 4; c.stroke(); }
};
K.life = (c, t, W, H, p, C) => {          // the six tracks as living beings that touch: each one moves the others
  const n = 6, cx = W / 2, cy = H / 2, R = Math.min(W, H) * .32;
  const pos = (k) => [cx + Math.cos(k * TAU / n + t * .2) * R, cy + Math.sin(k * TAU / n + t * .2) * R];
  for (let a = 0; a < n; a++) for (let b = a + 1; b < n; b++) { const s = Math.sin(t * .9 + a * 2 + b) * .5 + .5; if (s < .55) continue; const [x1, y1] = pos(a), [x2, y2] = pos(b);
    c.beginPath(); c.moveTo(x1, y1); c.lineTo(x2, y2); c.strokeStyle = alpha(C.a2, (s - .55) * 2); c.lineWidth = 1.5; c.stroke(); }
  for (let k = 0; k < n; k++) { const [x, y] = pos(k), b = 1 + .25 * Math.sin(t * 2.2 + k * 1.3); dot(c, x, y, 11 * b, alpha(C.acc, .85), 10); text(c, `${k + 1}`, x, y, C.bg || "#000", 10); }
};
K.swarm = (c, t, W, H, p, C) => {         // particles following a point: closer, more notes
  const tx = W / 2 + Math.cos(t * .7) * W * .3, ty = H / 2 + Math.sin(t * 1.1) * H * .3;
  ring(c, tx, ty, 14, C.fg, 2);
  for (let i = 0; i < 40; i++) { const a = t * (.5 + hash(i)) + i, r = 20 + hash(i * 3) * 70 * (1 + .3 * Math.sin(t + i)); dot(c, tx + Math.cos(a) * r, ty + Math.sin(a * 1.3) * r * .6, 2.5, alpha(i % 4 ? C.a2 : C.acc, .8)); }
};
K.prox = K.swarm;
K.velocity = (c, t, W, H, p, C) => {      // how hard: soft and loud hits, the sound brighter with force
  for (let i = 0; i < 12; i++) { const v = .2 + .8 * hash(i + Math.floor(t * 3) * 13), x = 30 + i * (W - 60) / 12, h = (H - 60) * v, on = i === Math.floor(t * 3) % 12;
    c.fillStyle = alpha(on ? C.acc : C.a2, .3 + .7 * v); c.fillRect(x, H - 24 - h, (W - 60) / 12 - 8, h); }
};
// GENERATORS -------------------------------------------------------------------------------------------------
K.gen = (c, t, W, H, p, C) => {           // a song growing: parts appear, tracks fill in, the form takes shape
  const parts = ["INTRO", "A", "B", "A", "C", "OUTRO"], grow = fract(t / 6) * 6, pw = (W - 40) / parts.length;
  parts.forEach((s, k) => { const a = Math.min(1, Math.max(0, grow - k)); if (a <= 0) return;
    text(c, s, 20 + (k + .5) * pw, 14, alpha(C.fg, a), 10);
    for (let r = 0; r < 6; r++) if (hash(k * 11 + r) < .3 + k * .1) box(c, 20 + k * pw + 2, 26 + r * (H - 36) / 6 + 2, pw * a - 4, (H - 36) / 6 - 4, null, alpha(r % 2 ? C.acc : C.a2, .3 + .5 * a)); });
};
K.dice = (c, t, W, H, p, C) => {          // a roll: values jump, then settle on a new set
  const roll = fract(t / 2) < .4, seed = Math.floor(t / 2);
  for (let k = 0; k < 6; k++) { const v = roll ? hash(k + t * 40) : hash(k + seed * 7), x = 30 + k * (W - 60) / 6;
    box(c, x, H / 2 - 40, (W - 60) / 6 - 10, 80, alpha(C.fg, .3)); c.fillStyle = roll ? C.a2 : C.acc; c.fillRect(x + 4, H / 2 + 36 - 72 * v, (W - 60) / 6 - 18, 72 * v); }
  text(c, roll ? "⚄ ⚂ ⚅" : "KEEP · CANCEL", W / 2, H - 16, C.fg, 13);
};
K.undo = (c, t, W, H, p, C) => {          // a change, and back
  const back = fract(t / 3) > .5, x = back ? 1 - (fract(t / 3) - .5) * 2 : fract(t / 3) * 2;
  trace(c, (u) => Math.sin(TAU * 2 * u) * (1 - x) * .7 + saw(u * 3) * x * .7, 20, H * .15, W - 40, H * .6, C.acc, 2.5);
  text(c, back ? "UNDO ↶" : "CHANGE", W / 2, H - 16, back ? C.a2 : C.fg, 13);
};
// THE CONCEPT AND THE SONG -----------------------------------------------------------------------------------
K.tracks = (c, t, W, H, p, C) => {        // six tracks, each its own engine, playing together
  const n = p.n || 6;
  for (let i = 0; i < n; i++) { const y = 14 + i * (H - 28) / n, hh = (H - 28) / n - 6;
    text(c, `T${i + 1}`, 22, y + hh / 2, C.dim, 10);
    trace(c, (x) => Math.sin(TAU * (2 + i) * x + t * (1 + i * .3)) * (.4 + .4 * Math.abs(Math.sin(t + i))), 40, y, W - 60, hh, i % 2 ? C.a2 : C.acc, 1.8); }
};
K.pages = (c, t, W, H, p, C) => {         // a section's pages in a chain, the button walks them, the knobs follow
  const n = 5, cur = Math.floor(t / 1.2) % n;
  for (let i = 0; i < n; i++) { const x = 20 + i * (W - 40) / n; box(c, x + 4, H * .25, (W - 40) / n - 8, H * .5, i === cur ? C.acc : alpha(C.fg, .25), i === cur ? alpha(C.acc, .15) : null);
    for (let k = 0; k < 4; k++) ring(c, x + 14 + k * ((W - 40) / n - 28) / 3, H * .62, 5, i === cur ? C.fg : C.dim, 1.5); }
  text(c, "→", 20 + (cur + 1) * (W - 40) / n, H * .18, C.fg, 16);
};
K.screen = (c, t, W, H, p, C) => {        // the screen: a 240 × 240 window where the sound is drawn
  const s = Math.min(W, H) * .8, x0 = (W - s) / 2, y0 = (H - s) / 2;
  box(c, x0, y0, s, s, alpha(C.fg, .4), alpha(C.fg, .03));
  K.creature(c, t, W, H, { legs: 5 }, C);
};
K.save = (c, t, W, H, p, C) => {          // the music kept: a flash of the slots filling
  const k = Math.floor(t / .8) % 5;
  ["SONG", "PROJECT", "SOUND", "USER"].forEach((s, i) => { box(c, 30 + i * (W - 60) / 4, H * .35, (W - 60) / 4 - 12, H * .3, i < k ? C.acc : alpha(C.fg, .3), i < k ? alpha(C.acc, .25) : null); text(c, s, 30 + (i + .5) * (W - 60) / 4 - 6, H * .5, C.fg, 11); });
};
K.song = (c, t, W, H, p, C) => {          // sections in order, the playhead going through them
  const secs = ["A", "A", "B", "A", "C", "B"], w = (W - 40) / secs.length, head = (t * .4) % secs.length;
  secs.forEach((s, i) => { box(c, 20 + i * w + 3, H * .3, w - 6, H * .4, alpha(C.fg, .3), Math.floor(head) === i ? alpha(C.acc, .4) : null); text(c, s, 20 + (i + .5) * w, H / 2, C.fg, 18); });
  c.fillStyle = C.acc; c.fillRect(20 + head * w, H * .25, 3, H * .5);
};
K.scene = (c, t, W, H, p, C) => {         // one key: the whole band changes to another state
  const k = Math.floor(t / 1.5) % 4, col = [C.acc, C.a2, C.fg, C.acc][k];
  for (let i = 0; i < 6; i++) box(c, 20 + i * (W - 40) / 6 + 4, H * .25, (W - 40) / 6 - 8, H * .4, alpha(C.fg, .25), alpha(col, .2 + .6 * hash(i + k * 9)));
  text(c, `SCENE ${k + 1}`, W / 2, H * .85, C.fg, 14);
};
K.keys = K.scale;

/* a kind's drawing, or the closest one (an unknown kind: the waveform) */
export function anim(kind) { return K[kind] || K[String(kind || "").replace(/[^a-z]/g, "")] || K.osc; }
export const KINDS = Object.keys(K);

/* one animation on one canvas: sized to its box (sharp on a Retina screen), running only while seen */
export class Player {
  constructor(canvas) {
    this.c = canvas; this.ctx = canvas.getContext("2d"); this.kind = "osc"; this.params = {}; this.raf = 0; this.t0 = performance.now();
    this.still = matchMedia("(prefers-reduced-motion: reduce)").matches;
    this.seen = true;
    if ("IntersectionObserver" in window) new IntersectionObserver((e) => { this.seen = e[0].isIntersecting; if (this.seen) this.go(); }).observe(canvas);
  }
  palette() {
    const s = getComputedStyle(this.c), v = (n, d) => (s.getPropertyValue(n).trim() || d);
    return { fg: v("--fg", "#efe6c8"), dim: v("--dim", "#a39bb6"), acc: v("--accent", "#ff7a1a"), a2: v("--accent-2", "#ff9a8a"), bg: v("--bg", "#050409") };
  }
  show(kind, params = {}) { this.kind = kind; this.params = params || {}; this.t0 = performance.now(); this.C = this.palette(); this.go(); }
  go() {
    if (this.raf) return;
    const step = () => {
      this.raf = 0;
      const r = this.c.getBoundingClientRect(), d = Math.min(2, devicePixelRatio || 1), W = Math.max(1, r.width), H = Math.max(1, r.height);
      if (this.c.width !== Math.round(W * d)) { this.c.width = Math.round(W * d); this.c.height = Math.round(H * d); }
      const c = this.ctx, VW = Math.max(W, 520), k = W / VW;   // (a phone's small stage: drawn as a wider one, scaled down)
      c.setTransform(d * k, 0, 0, d * k, 0, 0); c.clearRect(0, 0, VW, H / k);
      try { anim(this.kind)(c, this.still ? 1.3 : (performance.now() - this.t0) / 1000, VW, H / k, this.params, this.C || this.palette()); } catch (_) {}
      if (!this.still && this.seen && document.contains(this.c)) this.raf = requestAnimationFrame(step);
    };
    this.raf = requestAnimationFrame(step);
  }
}
