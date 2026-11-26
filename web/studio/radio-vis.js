/* JIANT Studio's Radio: an ecosystem that lives on the music. Six species, one per track (the firmware's track colours:
 * T1 yellow, T2 orange, T3 red, T4 magenta, T5 violet, T6 blue), each moved by its track's level (the emulator's stems):
 *   T1 protozoa (amoebas that swell on a hit)        T2 a school of fish (faster, tighter as it plays)
 *   T3 a flock (scatters on a hit)                   T4 a colony (cells added and lost with the level, a wave round it)
 *   T5 jellyfish (a thrust on a hit, then sinking)   T6 plankton in a current (bright with the level, a ring on a hit)
 * Light: one canvas, ~300 beings, trails by fading; FM drawn smooth and glowing, BYTE pixelated (a quarter-size canvas
 * scaled up). No audio: they breathe on their own. feed(levels[6]) about ten times a second; event("up"|"down"|"next") */
const TAU = Math.PI * 2, rnd = (a, b) => a + Math.random() * (b - a), clamp = (v, a, b) => Math.max(a, Math.min(b, v));
export const TRACK_COLORS = ["#ffe14d", "#ff8a1f", "#ff4a5a", "#ff4ad0", "#9d6bff", "#3b9cff"];

export class Ecosystem {
  constructor(canvas, skin = "fm") {
    this.cv = canvas; this.skin = skin; this.on = false; this.t = 0; this.raf = 0;
    this.lvl = new Float32Array(6); this.env = new Float32Array(6); this.avg = new Float32Array(6).fill(0.05); this.hit = new Float32Array(6);
    this.fedAt = 0; this.bloom = 0; this.scatter = 0; this.wipe = 0;
    this.off = document.createElement("canvas");
    this.resize(); this.populate();
    this.ro = new ResizeObserver(() => this.resize()); this.ro.observe(canvas);
  }
  setSkin(s) { if (s !== this.skin) { this.skin = s; this.resize(); } }
  resize() {
    const r = this.cv.getBoundingClientRect(), dpr = Math.min(1.5, devicePixelRatio || 1);
    this.w = Math.max(200, r.width); this.h = Math.max(160, r.height);
    this.cv.width = Math.round(this.w * dpr); this.cv.height = Math.round(this.h * dpr);
    this.px = this.skin === "byte" ? 4 : 1;                       // BYTE: drawn at a quarter, scaled up hard
    this.off.width = Math.ceil(this.w / this.px); this.off.height = Math.ceil(this.h / this.px);
    this.g = this.off.getContext("2d"); this.dpr = dpr;
    if (this.fish) this.wrapAll();
  }
  populate() {
    const W = () => rnd(0, this.w), H = () => rnd(0, this.h);
    this.amoebas = Array.from({ length: 6 }, () => ({ x: W(), y: H(), r: rnd(16, 30), a: rnd(0, TAU), ph: rnd(0, TAU), vx: rnd(-.2, .2), vy: rnd(-.2, .2) }));
    const boid = () => ({ x: W(), y: H(), vx: rnd(-1, 1), vy: rnd(-1, 1) });
    this.fish = Array.from({ length: 56 }, boid); this.birds = Array.from({ length: 40 }, boid);
    this.colony = { x: this.w * .7, y: this.h * .35, a: 0, cells: Array.from({ length: 40 }, (_, i) => ({ k: i, s: 0 })) };
    this.jellies = Array.from({ length: 5 }, () => ({ x: W(), y: H(), vy: 0, ph: rnd(0, TAU), r: rnd(10, 18) }));
    this.plankton = Array.from({ length: 160 }, () => ({ x: W(), y: H(), z: rnd(.3, 1) }));
    this.rings = [];
  }
  wrapAll() { for (const l of [this.fish, this.birds, this.jellies, this.plankton, this.amoebas]) for (const b of l) { b.x = clamp(b.x, 0, this.w); b.y = clamp(b.y, 0, this.h); } }
  /* the tracks' levels (0..1, already normalized), about ten times a second */
  feed(levels) {
    this.fedAt = performance.now();
    for (let i = 0; i < 6; i++) {
      const v = clamp(levels[i] || 0, 0, 1.5);
      if (v > this.avg[i] * 1.5 + 0.08 && v - this.lvl[i] > 0.12) this.onHit(i, v);
      this.avg[i] = this.avg[i] * 0.92 + v * 0.08; this.lvl[i] = v;
    }
  }
  onHit(i, v) {
    this.hit[i] = 1;
    if (i === 2) for (const b of this.birds) { const dx = b.x - this.w / 2, dy = b.y - this.h / 2, d = Math.hypot(dx, dy) || 1; b.vx += dx / d * 3 * v; b.vy += dy / d * 3 * v; }
    if (i === 4) for (const j of this.jellies) j.vy -= 1.6 + v;
    if (i === 5 && this.rings.length < 12) this.rings.push({ x: rnd(0, this.w), y: rnd(0, this.h), r: 4, a: 1, c: 5 });
  }
  event(kind) {
    if (kind === "up") { this.bloom = 1; for (let k = 0; k < 4; k++) this.rings.push({ x: this.w / 2, y: this.h / 2, r: 6 + k * 14, a: 1, c: k % 6 }); }
    if (kind === "down") { this.scatter = 1; for (const l of [this.fish, this.birds]) for (const b of l) { b.vx += rnd(-4, 4); b.vy += rnd(-4, 4); } }
    if (kind === "next") this.wipe = 1;
  }
  start() { if (this.on) return; this.on = true; const step = (ts) => { if (!this.on) return; this.frame(ts); this.raf = requestAnimationFrame(step); }; this.raf = requestAnimationFrame(step); }
  stop() { this.on = false; cancelAnimationFrame(this.raf); }
  frame(ts) {
    const dt = Math.min(2, this.last ? (ts - this.last) / 16.7 : 1); this.last = ts; this.t += dt / 60;
    const idle = performance.now() - this.fedAt > 1500;          // no music: a slow breath of their own
    for (let i = 0; i < 6; i++) {
      const target = idle ? 0.12 + 0.08 * Math.sin(this.t * (0.7 + i * 0.23) + i) : this.lvl[i];
      this.env[i] += (target - this.env[i]) * (target > this.env[i] ? 0.5 : 0.08) * dt;
      this.hit[i] *= Math.pow(0.88, dt);
    }
    this.bloom *= Math.pow(0.95, dt); this.scatter *= Math.pow(0.95, dt); this.wipe *= Math.pow(0.9, dt);
    const g = this.g, P = this.px, byte = this.skin === "byte";
    g.setTransform(1 / P, 0, 0, 1 / P, 0, 0);
    const total = this.env.reduce((a, b) => a + b, 0) / 6;
    g.globalCompositeOperation = "source-over";
    g.fillStyle = byte ? `rgba(0,0,0,${0.5 + this.wipe * 0.5})` : `rgba(3,3,12,${0.24 + this.wipe * 0.6})`;
    g.fillRect(0, 0, this.w, this.h);
    if (!byte) g.globalCompositeOperation = "lighter";
    this.drawPlankton(g, dt); this.drawWeb(g); this.drawColony(g, dt); this.drawAmoebas(g, dt);
    this.drawBoids(g, this.fish, 1, dt, { align: .06, cohere: .004, sep: 14, speed: 1.2 + this.env[1] * 3.5, len: 6 });
    this.drawBoids(g, this.birds, 2, dt, { align: .03, cohere: .002, sep: 22, speed: 1.6 + this.env[2] * 3, len: 4, wing: true });
    this.drawJellies(g, dt); this.drawRings(g, dt);
    const c = this.cv.getContext("2d");
    c.setTransform(1, 0, 0, 1, 0, 0); c.imageSmoothingEnabled = !byte;
    c.globalAlpha = 1; c.drawImage(this.off, 0, 0, this.cv.width, this.cv.height);
    this.drawHud(c, byte);
  }
  /* the HUD (crisp, on top): corner brackets, each species tagged with a leader line, the six levels as a readout */
  drawHud(c, byte) {
    const d = this.dpr, W = this.w, H = this.h; c.save(); c.scale(d, d);
    c.strokeStyle = "rgba(235,235,255,.45)"; c.lineWidth = 1;
    for (const [x, y, sx, sy] of [[10, 10, 1, 1], [W - 10, 10, -1, 1], [10, H - 10, 1, -1], [W - 10, H - 10, -1, -1]]) { c.beginPath(); c.moveTo(x, y + sy * 16); c.lineTo(x, y); c.lineTo(x + sx * 16, y); c.stroke(); }
    c.font = `${byte ? 9 : 10}px ui-monospace, "JetBrains Mono", monospace`; c.textBaseline = "middle";
    const NAMES = ["T1 PROTOZOA", "T2 SCHOOL", "T3 FLOCK", "T4 COLONY", "T5 MEDUSAE", "T6 PLANKTON"];
    const where = [this.amoebas[0], this.fish[0], this.birds[0], this.colony, this.jellies[0], this.plankton[0]];
    for (let i = 0; i < 6; i++) {
      const p = where[i]; if (!p) continue;
      if (W < 520) { c.strokeStyle = this.col(i, 0.8); c.beginPath(); c.arc(p.x, p.y, 3 + this.hit[i] * 8, 0, TAU); c.stroke(); continue; }
      const tx = clamp(p.x + (p.x > W / 2 ? -150 : 60), 14, W - 130), ty = clamp(p.y - 40, 70 + i * 18, H - 250 + i * 18), lab = `${NAMES[i]} ${this.env[i].toFixed(2)}`;
      c.strokeStyle = this.col(i, 0.35 + this.env[i] * .5); c.setLineDash([2, 3]); c.beginPath(); c.moveTo(p.x, p.y); c.lineTo(tx + (tx < p.x ? 116 : 0), ty); c.stroke(); c.setLineDash([]);
      c.fillStyle = this.col(i, 0.9); c.fillRect(tx, ty - 7, 4, 14);
      c.fillStyle = "rgba(5,4,9,.7)"; c.fillRect(tx + 6, ty - 7, 110, 14);
      c.fillStyle = this.col(i, 1); c.fillText(lab, tx + 9, ty + .5);
      c.strokeStyle = this.col(i, 0.8); c.beginPath(); c.arc(p.x, p.y, 3 + this.hit[i] * 8, 0, TAU); c.stroke();
    }
    const bx = W - 92, by = 64;                                    // the readout: six bars, their tracks' colours
    c.strokeStyle = "rgba(235,235,255,.3)"; c.strokeRect(bx - 6, by - 6, 80, 46);
    for (let i = 0; i < 6; i++) { c.fillStyle = this.col(i, .9); const hgt = 4 + this.env[i] * 30; c.fillRect(bx + i * 12, by + 34 - hgt, 8, hgt); }
    c.restore();
  }
  col(i, a) { const h = TRACK_COLORS[i]; return a >= 1 ? h : h + Math.round(clamp(a, 0, 1) * 255).toString(16).padStart(2, "0"); }
  drawAmoebas(g, dt) {                                          // point clouds (the stippled look), a nucleus
    const e = this.env[0], k = this.hit[0];
    this.dots = this.dots || Array.from({ length: 70 }, () => { const a = rnd(0, TAU), r = Math.sqrt(Math.random()); return [a, r]; });
    for (const m of this.amoebas) {
      m.ph += dt * (0.02 + e * 0.08); m.a += dt * 0.004;
      m.x += (m.vx + Math.cos(m.a) * .25) * dt; m.y += (m.vy + Math.sin(m.a * 1.3) * .25) * dt;
      if (m.x < -40) m.x = this.w + 40; if (m.x > this.w + 40) m.x = -40; if (m.y < -40) m.y = this.h + 40; if (m.y > this.h + 40) m.y = -40;
      const R = m.r * (0.8 + e * 0.9 + k * 0.6 + this.bloom * 0.6), edge = (a) => R * (1 + 0.18 * Math.sin(a * 3 + m.ph * 2) + 0.12 * Math.sin(a * 5 - m.ph * 3));
      g.fillStyle = this.col(0, 0.35 + e * 0.6);
      for (const [a, r] of this.dots) { const rr = edge(a) * (r * .92 + .08 * Math.sin(m.ph * 4 + a * 7)); g.fillRect(m.x + Math.cos(a) * rr, m.y + Math.sin(a) * rr, 1.4, 1.4); }
      g.strokeStyle = this.col(0, 0.25 + e * 0.5); g.lineWidth = .8; g.beginPath();
      for (let s = 0; s <= 28; s++) { const a = s / 28 * TAU, rr = edge(a); s ? g.lineTo(m.x + Math.cos(a) * rr, m.y + Math.sin(a) * rr) : g.moveTo(m.x + rr, m.y); }
      g.stroke();
      g.fillStyle = this.col(0, 0.9); g.beginPath(); g.arc(m.x + Math.cos(m.ph) * R * .25, m.y + Math.sin(m.ph) * R * .25, 1.5 + R * .1, 0, TAU); g.fill();
    }
  }
  /* the web between them: nearby beings of different species joined by hairlines, brighter as both play */
  drawWeb(g) {
    const N = [];
    for (const m of this.amoebas) N.push([m.x, m.y, 0]);
    for (let i = 0; i < this.fish.length; i += 4) N.push([this.fish[i].x, this.fish[i].y, 1]);
    for (let i = 0; i < this.birds.length; i += 4) N.push([this.birds[i].x, this.birds[i].y, 2]);
    N.push([this.colony.x, this.colony.y, 3]);
    for (const j of this.jellies) N.push([j.x, j.y, 4]);
    for (let i = 0; i < this.plankton.length; i += 16) N.push([this.plankton[i].x, this.plankton[i].y, 5]);
    this.nodes = N;
    g.lineWidth = .6;
    for (let a = 0; a < N.length; a++) for (let b = a + 1; b < N.length; b++) {
      if (N[a][2] === N[b][2]) continue;
      const d = Math.hypot(N[a][0] - N[b][0], N[a][1] - N[b][1]); if (d > 150) continue;
      const al = (1 - d / 150) * (0.08 + (this.env[N[a][2]] + this.env[N[b][2]]) * 0.25);
      g.strokeStyle = `rgba(235,235,255,${al.toFixed(3)})`; g.beginPath(); g.moveTo(N[a][0], N[a][1]); g.lineTo(N[b][0], N[b][1]); g.stroke();
    }
  }
  drawBoids(g, L, ci, dt, o) {
    const e = this.env[ci], n = L.length;
    for (let a = 0; a < n; a++) {
      const b = L[a]; let ax = 0, ay = 0, cx = 0, cy = 0, sx = 0, sy = 0, m = 0;
      for (let j = 0; j < n; j++) {
        if (j === a) continue;
        const c = L[j], dx = c.x - b.x, dy = c.y - b.y, d2 = dx * dx + dy * dy;
        if (d2 > 4900) continue;
        m++; ax += c.vx; ay += c.vy; cx += dx; cy += dy;
        if (d2 < o.sep * o.sep) { sx -= dx / (d2 + 1) * 20; sy -= dy / (d2 + 1) * 20; }
      }
      if (m) { b.vx += (ax / m - b.vx) * o.align * (1 + e) + cx / m * o.cohere * (1 + e * 2); b.vy += (ay / m - b.vy) * o.align * (1 + e) + cy / m * o.cohere * (1 + e * 2); }
      b.vx += sx * .05 + Math.sin(this.t * .5 + ci) * .02; b.vy += sy * .05 + Math.cos(this.t * .4 + ci) * .02;
      const sp = Math.hypot(b.vx, b.vy) || 1, want = o.speed * (1 + this.scatter);
      b.vx = b.vx / sp * (sp + (want - sp) * .1); b.vy = b.vy / sp * (sp + (want - sp) * .1);
      b.x += b.vx * dt; b.y += b.vy * dt;
      if (b.x < -10) b.x += this.w + 20; if (b.x > this.w + 10) b.x -= this.w + 20; if (b.y < -10) b.y += this.h + 20; if (b.y > this.h + 10) b.y -= this.h + 20;
      const ux = b.vx / (Math.hypot(b.vx, b.vy) || 1), uy = b.vy / (Math.hypot(b.vx, b.vy) || 1), L2 = o.len * (1 + e * .8);
      g.strokeStyle = this.col(ci, 0.45 + e * 0.55); g.lineWidth = o.wing ? 1.4 : 2.2; g.beginPath();
      if (o.wing) { const f = Math.sin(this.t * 14 + a) * (2 + e * 3); g.moveTo(b.x - ux * L2 - uy * (L2 + f), b.y - uy * L2 + ux * (L2 + f)); g.lineTo(b.x, b.y); g.lineTo(b.x - ux * L2 + uy * (L2 + f), b.y - uy * L2 - ux * (L2 + f)); }
      else { g.moveTo(b.x - ux * L2 * 2, b.y - uy * L2 * 2); g.lineTo(b.x, b.y); }
      g.stroke();
    }
  }
  drawColony(g, dt) {
    const C = this.colony, e = this.env[3], k = this.hit[3];
    C.a += dt * (0.002 + e * 0.01); C.x = this.w * (.62 + .2 * Math.sin(this.t * .07)); C.y = this.h * (.4 + .18 * Math.cos(this.t * .05));
    const alive = Math.round(4 + e * 36);
    for (const c of C.cells) {
      c.s += ((c.k < alive ? 1 : 0) - c.s) * 0.06 * dt;
      if (c.s < 0.02) continue;
      const ring = Math.floor((Math.sqrt(1 + 4 * c.k / 3) - 1) / 2) + 1, idx = c.k - 3 * ring * (ring - 1), cnt = 6 * ring;   // hex rings round a centre
      const a = (idx / cnt) * TAU + C.a * (ring % 2 ? 1 : -1), rad = c.k ? ring * 15 * (1 + k * .3) : 0;
      const x = C.x + Math.cos(a) * rad, y = C.y + Math.sin(a) * rad;
      const wave = 0.6 + 0.4 * Math.sin(this.t * 6 - ring * 1.2 - idx * .3);
      g.fillStyle = this.col(3, (0.15 + e * 0.5) * c.s * wave); g.strokeStyle = this.col(3, 0.7 * c.s);
      g.beginPath(); g.arc(x, y, (5 + e * 3 + this.bloom * 4) * c.s, 0, TAU); g.fill(); g.lineWidth = 1; g.stroke();
    }
  }
  drawJellies(g, dt) {
    const e = this.env[4];
    for (const j of this.jellies) {
      j.ph += dt * (0.05 + e * 0.1); j.vy += 0.015 * dt; j.vy *= Math.pow(0.97, dt); j.y += j.vy * dt; j.x += Math.sin(j.ph * .3) * .3 * dt;
      if (j.y > this.h + 40) { j.y = -30; j.x = rnd(0, this.w); } if (j.y < -60) j.y = this.h + 30;
      const R = j.r * (1 + e * .5 + this.bloom * .5), sq = 1 + Math.min(.4, Math.max(0, -j.vy) * .15);
      g.fillStyle = this.col(4, 0.15 + e * 0.35); g.strokeStyle = this.col(4, 0.6 + e * 0.4); g.lineWidth = 1.5;
      g.beginPath(); g.ellipse(j.x, j.y, R / sq, R * .7 * sq, 0, Math.PI, TAU); g.closePath(); g.fill(); g.stroke();
      for (let t = 0; t < 4; t++) {
        const x0 = j.x - R / sq + (t + .5) * (2 * R / sq) / 4; g.beginPath(); g.moveTo(x0, j.y);
        for (let s = 1; s <= 6; s++) g.lineTo(x0 + Math.sin(j.ph * 2 + s * .9 + t) * (2 + e * 4), j.y + s * R * .45);
        g.stroke();
      }
    }
  }
  drawPlankton(g, dt) {
    const e = this.env[5];
    g.fillStyle = this.col(5, 0.35 + e * 0.65);
    for (const p of this.plankton) {
      const a = Math.sin(p.x * .006 + this.t * .3) + Math.cos(p.y * .007 - this.t * .2);
      p.x += Math.cos(a * 2) * (0.3 + e * 1.6) * p.z * dt; p.y += Math.sin(a * 2) * (0.3 + e * 1.6) * p.z * dt;
      if (p.x < 0) p.x += this.w; if (p.x > this.w) p.x -= this.w; if (p.y < 0) p.y += this.h; if (p.y > this.h) p.y -= this.h;
      const s = (1 + e * 2.5) * p.z; g.fillRect(p.x, p.y, s, s);
    }
  }
  drawRings(g, dt) {
    this.rings = this.rings.filter((r) => r.a > 0.02);
    for (const r of this.rings) { r.r += (1.5 + r.r * .03) * dt; r.a *= Math.pow(0.95, dt); g.strokeStyle = this.col(r.c, r.a); g.lineWidth = 2; g.beginPath(); g.arc(r.x, r.y, r.r, 0, TAU); g.stroke(); }
  }
}
/* the stems' planes (Float32Array per track) -> levels 0..1, each track against its own recent loudest (so a quiet
   track still moves its species) */
export class Levels {
  constructor() { this.peak = new Float32Array(6).fill(0.05); }
  of(planes) {
    const out = new Float32Array(6);
    for (let i = 0; i < 6 && i < planes.length; i++) {
      const p = planes[i]; let s = 0; for (let k = 0; k < p.length; k += 4) s += p[k] * p[k];
      const rms = Math.sqrt(s / Math.max(1, p.length / 4));
      this.peak[i] = Math.max(rms, this.peak[i] * 0.995, 0.01);
      out[i] = rms < 0.002 ? 0 : rms / this.peak[i];
    }
    return out;
  }
}
