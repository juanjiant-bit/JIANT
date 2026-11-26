/* JIANT Studio's Radio: a specimen tank that lives on the music, drawn as a lab readout (stippled point clouds, a
 * wireframe box in perspective, particle paths coloured by speed, HUD tags, a heat map of the tracks, a signal trace).
 * Six species, one per track (its stems' level): T1 protozoa (point-cloud cells), T2 a school (paths), T3 particles
 * falling through the strata (impulsion), T4 a colony (wireframe spheres), T5 medusae (stippled domes, dotted
 * tentacles), T6 the forest (columns of points rising from a stippled floor). FM smooth, BYTE pixelated.
 * feed(levels[6]) about ten times a second; event("up"|"down"|"next"); setInfo({ genre, seed, mood, form }) */
const TAU = Math.PI * 2, rnd = (a, b) => a + Math.random() * (b - a), clamp = (v, a, b) => Math.max(a, Math.min(b, v));
export const TRACK_COLORS = ["#ffe14d", "#ff8a1f", "#ff4a5a", "#ff4ad0", "#9d6bff", "#3b9cff"];
const NAMES = ["PROTOZOA", "SCHOOL", "IMPULSION", "COLONY", "MEDUSAE", "FOREST"];
const mix = (a, b, t) => a.map((v, i) => Math.round(v + (b[i] - v) * t));
const SPEED = [[246, 235, 200], [242, 193, 78], [224, 57, 74]];   // the paths: cream (slow) .. yellow .. red (fast)
const speedCol = (t, a) => { t = clamp(t, 0, 1); const c = t < .5 ? mix(SPEED[0], SPEED[1], t * 2) : mix(SPEED[1], SPEED[2], t * 2 - 1); return `rgba(${c},${a})`; };
const jet = (v) => { v = clamp(v, 0, 1); const r = clamp(1.5 - Math.abs(4 * v - 3), 0, 1), g = clamp(1.5 - Math.abs(4 * v - 2), 0, 1), b = clamp(1.5 - Math.abs(4 * v - 1), 0, 1); return `rgb(${r * 255 | 0},${g * 255 | 0},${b * 255 | 0})`; };
const sphere = (n) => Array.from({ length: n }, () => { const u = rnd(-1, 1), a = rnd(0, TAU), s = Math.sqrt(1 - u * u); return [s * Math.cos(a), u, s * Math.sin(a)]; });

export class Ecosystem {
  constructor(canvas, skin = "fm") {
    this.cv = canvas; this.skin = skin; this.on = false; this.t = 0; this.raf = 0;
    this.lvl = new Float32Array(6); this.env = new Float32Array(6); this.avg = new Float32Array(6).fill(0.05); this.hit = new Float32Array(6);
    this.hist = Array.from({ length: 6 }, () => new Float32Array(64)); this.histAt = 0;
    this.fedAt = 0; this.bloom = 0; this.scatter = 0; this.wipe = 0; this.info = {};
    this.off = document.createElement("canvas");
    this.populate(); this.resize();
    this.ro = new ResizeObserver(() => this.resize()); this.ro.observe(canvas);
  }
  setSkin(s) { if (s !== this.skin) { this.skin = s; this.resize(); } }
  setInfo(i) { this.info = i || {}; }
  resize() {
    const r = this.cv.getBoundingClientRect(), dpr = Math.min(1.5, devicePixelRatio || 1);
    this.w = Math.max(240, r.width); this.h = Math.max(200, r.height); this.dpr = dpr;
    this.cv.width = Math.round(this.w * dpr); this.cv.height = Math.round(this.h * dpr);
    this.px = this.skin === "byte" ? 3 : 1;
    this.off.width = Math.ceil(this.w * (this.px === 1 ? dpr : 1 / this.px)); this.off.height = Math.ceil(this.h * (this.px === 1 ? dpr : 1 / this.px));
    this.g = this.off.getContext("2d");
    this.wide = this.w > 900;
    this.cx = this.wide ? this.w * 0.4 : this.w / 2; this.cy = this.h * (this.wide ? 0.52 : 0.44);
    this.S = Math.min(this.h * 0.2, (this.wide ? this.w * 0.62 : this.w) * 0.3);
  }
  populate() {
    this.cells = Array.from({ length: 5 }, () => ({ p: [rnd(-.6, .6), rnd(-.4, .9), rnd(-.6, .6)], r: rnd(.16, .26), ph: rnd(0, TAU), pts: sphere(110) }));
    this.school = Array.from({ length: 48 }, () => ({ p: [rnd(-.8, .8), rnd(-.9, .9), rnd(-.8, .8)], v: [rnd(-.01, .01), rnd(-.01, .01), rnd(-.01, .01)], tr: [] }));
    this.fall = Array.from({ length: 60 }, () => this.dropNew({}));
    this.colony = Array.from({ length: 26 }, (_, i) => ({ p: [rnd(-.45, .45), rnd(-.05, .45), rnd(-.45, .45)], r: rnd(.05, .11), s: 0, k: i }));
    this.jellies = Array.from({ length: 4 }, () => ({ p: [rnd(-.7, .7), rnd(.2, 1.1), rnd(-.7, .7)], vy: 0, ph: rnd(0, TAU), r: rnd(.12, .18) }));
    this.dome = sphere(90).filter((q) => q[1] > -.05);
    this.trunks = Array.from({ length: 6 }, (_, i) => ({ x: Math.cos(i / 6 * TAU + .4) * rnd(1.25, 1.9), z: Math.sin(i / 6 * TAU + .4) * rnd(1.25, 1.9), r: rnd(.07, .14),
      pts: Array.from({ length: 260 }, () => [rnd(0, TAU), Math.random()]) }));
    this.floor = Array.from({ length: 900 }, () => [rnd(-2.4, 2.4), rnd(-2.4, 2.4), Math.random()]);
  }
  dropNew(d) { d.p = [rnd(-.85, .85), rnd(1.05, 1.45), rnd(-.85, .85)]; d.vy = 0; d.tr = []; d.r = rnd(.008, .022); return d; }
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
    if (i === 1) for (const b of this.school) { b.v[0] += rnd(-.02, .02) * v; b.v[1] += rnd(-.02, .02) * v; b.v[2] += rnd(-.02, .02) * v; }
    if (i === 2) for (let k = 0; k < 8; k++) this.dropNew(this.fall[(Math.random() * this.fall.length) | 0]);
    if (i === 4) for (const j of this.jellies) j.vy += .012 + v * .01;
  }
  event(kind) {
    if (kind === "up") this.bloom = 1;
    if (kind === "down") { this.scatter = 1; for (const b of this.school) { b.v[0] += rnd(-.04, .04); b.v[2] += rnd(-.04, .04); } }
    if (kind === "next") this.wipe = 1;
  }
  start() { if (this.on) return; this.on = true; const step = (ts) => { if (!this.on) return; this.frame(ts); this.raf = requestAnimationFrame(step); }; this.raf = requestAnimationFrame(step); }
  stop() { this.on = false; cancelAnimationFrame(this.raf); }
  /* the camera: turning slowly round the tank, a little from above */
  proj(x, y, z) {
    const a = this.yaw, cp = this.pitch, x1 = x * Math.cos(a) - z * Math.sin(a), z1 = x * Math.sin(a) + z * Math.cos(a);
    const y1 = y * Math.cos(cp) - z1 * Math.sin(cp), z2 = y * Math.sin(cp) + z1 * Math.cos(cp), k = 4.2 / (4.2 + z2);
    return [this.cx + x1 * k * this.S, this.cy - y1 * k * this.S, k];
  }
  frame(ts) {
    const dt = Math.min(2, this.last ? (ts - this.last) / 16.7 : 1); this.last = ts; this.t += dt / 60;
    const idle = performance.now() - this.fedAt > 1500;
    for (let i = 0; i < 6; i++) {
      const target = idle ? 0.14 + 0.1 * Math.sin(this.t * (0.6 + i * 0.21) + i) : this.lvl[i];
      this.env[i] += (target - this.env[i]) * (target > this.env[i] ? 0.5 : 0.08) * dt;
      this.hit[i] *= Math.pow(0.88, dt);
    }
    if ((this.histAt += dt) > 6) { this.histAt = 0; for (let i = 0; i < 6; i++) { this.hist[i].copyWithin(0, 1); this.hist[i][63] = this.env[i]; } }
    this.bloom *= Math.pow(0.95, dt); this.scatter *= Math.pow(0.95, dt); this.wipe *= Math.pow(0.9, dt);
    this.yaw = this.t * 0.08 + Math.sin(this.t * 0.05) * 0.4; this.pitch = 0.28 + Math.sin(this.t * 0.04) * 0.08;
    const byte = this.skin === "byte", g = this.g, sc = byte ? 1 / this.px : this.dpr;
    g.setTransform(sc, 0, 0, sc, 0, 0);
    g.fillStyle = byte ? "#000" : `rgba(2,2,8,${0.78 + this.wipe * 0.22})`; g.fillRect(0, 0, this.w, this.h);
    this.drawFloor(g); this.drawTrunks(g); this.drawBox(g);
    this.drawColony(g); this.drawCells(g, dt); this.drawJellies(g, dt); this.drawSchool(g, dt); this.drawFall(g, dt);
    const c = this.cv.getContext("2d");
    c.setTransform(1, 0, 0, 1, 0, 0); c.imageSmoothingEnabled = !byte; c.globalAlpha = 1;
    c.drawImage(this.off, 0, 0, this.cv.width, this.cv.height);
    this.drawHud(c, byte);
  }
  dot(g, p, s, a, col = "246,240,226") { g.fillStyle = `rgba(${col},${a})`; g.fillRect(p[0], p[1], s, s); }
  drawFloor(g) {
    const e = this.env[5], t = this.t;
    for (const [x, z, n] of this.floor) {
      const d = Math.hypot(x, z); if (d > 2.4) continue;
      const y = -1.5 + Math.sin(x * 2.2 + t * 1.4) * Math.cos(z * 1.8 - t) * .04 * (1 + e * 4);
      const p = this.proj(x, y, z);
      this.dot(g, p, 1.2 * p[2], (0.12 + n * 0.25 + e * 0.25) * (1 - d / 2.6));
    }
  }
  drawTrunks(g) {                                                // the forest: columns of points, taller and brighter as T6 plays
    const e = this.env[5], H = 1.6 + e * 1.6 + this.hit[5] * .4;
    for (const tr of this.trunks) for (const [a, u] of tr.pts) {
      const y = -1.5 + u * H, flare = 1 + Math.max(0, .25 - u) * 3, r = tr.r * flare * (1 + .15 * Math.sin(u * 9 + a * 3));
      const p = this.proj(tr.x + Math.cos(a) * r, y, tr.z + Math.sin(a) * r), edge = Math.abs(Math.sin(a - this.yaw));
      this.dot(g, p, 1.4 * p[2], (0.3 + edge * 0.6) * (0.6 + e * 0.5) * (1 - u * .35));
    }
  }
  drawBox(g) {                                                    // the tank: a wireframe box, its strata dotted meshes
    const B = [-1, 1], Y = [-1.5, 1.5], P = (x, y, z) => this.proj(x, y, z);
    g.strokeStyle = "rgba(235,235,255,.38)"; g.lineWidth = .8; g.beginPath();
    for (const x of B) for (const z of B) { const a = P(x, Y[0], z), b = P(x, Y[1], z); g.moveTo(a[0], a[1]); g.lineTo(b[0], b[1]); }
    for (const y of Y) for (const [x0, z0, x1, z1] of [[-1, -1, 1, -1], [1, -1, 1, 1], [1, 1, -1, 1], [-1, 1, -1, -1]]) { const a = P(x0, y, z0), b = P(x1, y, z1); g.moveTo(a[0], a[1]); g.lineTo(b[0], b[1]); }
    g.stroke();
    for (const y of [1.0, 0.55, -0.9]) for (let i = -10; i <= 10; i += 2) for (let j = -10; j <= 10; j += 2) {
      const p = P(i / 10, y + Math.sin(i * .7 + j * .5 + this.t) * .015, j / 10); this.dot(g, p, 1, .16);
    }
  }
  drawCells(g, dt) {                                              // protozoa: point-cloud cells that swell on a hit
    const e = this.env[0], k = this.hit[0];
    for (const c of this.cells) {
      c.ph += dt * (0.02 + e * .06);
      c.p[0] += Math.cos(c.ph * .3) * .002 * dt; c.p[2] += Math.sin(c.ph * .27) * .002 * dt; c.p[1] += Math.sin(c.ph * .2) * .0015 * dt;
      for (let i = 0; i < 3; i++) c.p[i] = clamp(c.p[i], i === 1 ? -1.2 : -.8, i === 1 ? 1.2 : .8);
      const R = c.r * (0.8 + e * .8 + k * .5 + this.bloom * .5);
      for (const [x, y, z] of c.pts) {
        const wob = 1 + .2 * Math.sin(x * 4 + c.ph * 2) * Math.cos(y * 3 - c.ph);
        const p = this.proj(c.p[0] + x * R * wob, c.p[1] + y * R * wob * .85, c.p[2] + z * R * wob);
        this.dot(g, p, 1.4 * p[2], .35 + e * .55);
      }
      const n = this.proj(...c.p); g.fillStyle = speedCol(.3 + e * .7, .9); g.beginPath(); g.arc(n[0], n[1], 2.5 + R * 14 * n[2] * .3, 0, TAU); g.fill();
      c.sp = n;
    }
  }
  drawSchool(g, dt) {                                             // a school: paths through the tank, cream to red by speed
    const e = this.env[1], L = this.school, n = L.length, sp = 0.006 + e * 0.022;
    let cx = 0, cy = 0, cz = 0; for (const b of L) { cx += b.p[0]; cy += b.p[1]; cz += b.p[2]; } cx /= n; cy /= n; cz /= n;
    const tx = Math.cos(this.t * .3) * .6, ty = Math.sin(this.t * .23) * .8, tz = Math.sin(this.t * .3) * .6;
    for (const b of L) {
      b.v[0] += ((cx - b.p[0]) * .002 + (tx - b.p[0]) * .0015) * (1 + e); b.v[1] += ((cy - b.p[1]) * .002 + (ty - b.p[1]) * .0015) * (1 + e); b.v[2] += ((cz - b.p[2]) * .002 + (tz - b.p[2]) * .0015) * (1 + e);
      const m = Math.hypot(...b.v) || 1, want = sp * (1 + this.scatter);
      for (let i = 0; i < 3; i++) { b.v[i] = b.v[i] / m * (m + (want - m) * .08); b.p[i] += b.v[i] * dt; }
      b.p[0] = clamp(b.p[0], -.98, .98); b.p[1] = clamp(b.p[1], -1.45, 1.45); b.p[2] = clamp(b.p[2], -.98, .98);
      b.tr.push([...b.p]); if (b.tr.length > 14) b.tr.shift();
      const v = clamp(m / 0.028, 0, 1);
      g.lineWidth = .8; g.beginPath();
      b.tr.forEach((q, i) => { const p = this.proj(...q); i ? g.lineTo(p[0], p[1]) : g.moveTo(p[0], p[1]); });
      g.strokeStyle = speedCol(v, .55); g.stroke();
      const p = this.proj(...b.p); g.fillStyle = speedCol(v, .95); g.beginPath(); g.arc(p[0], p[1], 1.6 * p[2], 0, TAU); g.fill();
    }
    this.schoolC = this.proj(cx, cy, cz);
  }
  drawFall(g, dt) {                                               // impulsion: particles falling through the strata
    const e = this.env[2];
    for (const d of this.fall) {
      d.vy -= (0.0004 + e * 0.0012) * dt; d.vy *= .985; d.p[1] += d.vy * dt;
      d.p[0] += Math.sin(this.t * 2 + d.p[1] * 5) * .001 * dt;
      if (d.p[1] < -1.45) this.dropNew(d);
      d.tr.push([...d.p]); if (d.tr.length > 18) d.tr.shift();
      const v = clamp(-d.vy / 0.03, 0, 1);
      g.lineWidth = .7; g.beginPath(); d.tr.forEach((q, i) => { const p = this.proj(...q); i ? g.lineTo(p[0], p[1]) : g.moveTo(p[0], p[1]); });
      g.strokeStyle = speedCol(1 - v * .6, .35); g.stroke();
      const p = this.proj(...d.p), r = d.r * this.S * p[2] * (1 + e * .6);
      g.fillStyle = speedCol(clamp((d.p[1] + .9) / 2.2, 0, 1), .9); g.beginPath(); g.arc(p[0], p[1], r, 0, TAU); g.fill();
      g.strokeStyle = "rgba(255,255,255,.4)"; g.lineWidth = .5; g.beginPath(); g.arc(p[0], p[1], r * 1.5, 0, TAU); g.stroke();
    }
  }
  drawColony(g) {                                                 // a colony: wireframe spheres in the middle stratum
    const e = this.env[3], alive = Math.round(5 + e * 21);
    for (const c of this.colony) {
      c.s += ((c.k < alive ? 1 : 0) - c.s) * .06;
      if (c.s < .03) continue;
      const p = this.proj(c.p[0], c.p[1] + Math.sin(this.t * 2 + c.k) * .02, c.p[2]), R = c.r * this.S * p[2] * c.s * (1 + this.hit[3] * .4 + this.bloom * .4);
      g.strokeStyle = `rgba(246,235,200,${.25 + e * .5})`; g.lineWidth = .6;
      g.beginPath(); g.arc(p[0], p[1], R, 0, TAU); g.stroke();
      g.beginPath(); g.ellipse(p[0], p[1], R, R * .35, 0, 0, TAU); g.stroke();
      g.beginPath(); g.ellipse(p[0], p[1], R * .35, R, 0, 0, TAU); g.stroke();
      g.fillStyle = `rgba(246,235,200,${.05 + e * .15})`; g.beginPath(); g.arc(p[0], p[1], R, 0, TAU); g.fill();
    }
    this.colonyC = this.proj(0, .2, 0);
  }
  drawJellies(g, dt) {                                            // medusae: stippled domes, dotted tentacles
    const e = this.env[4];
    for (const j of this.jellies) {
      j.ph += dt * (0.04 + e * .1); j.vy -= .0003 * dt; j.vy *= .97; j.p[1] += j.vy * dt;
      if (j.p[1] < -1.3) j.p[1] = 1.3; if (j.p[1] > 1.4) j.vy = -.003;
      const R = j.r * (1 + e * .5 + this.bloom * .4), pulse = 1 + Math.sin(j.ph * 3) * .08 * (1 + e);
      for (const [x, y, z] of this.dome) { const p = this.proj(j.p[0] + x * R * pulse, j.p[1] + y * R * .7, j.p[2] + z * R * pulse); this.dot(g, p, 1.3 * p[2], .4 + e * .5, "205,195,255"); }
      for (let t = 0; t < 6; t++) {
        const a = t / 6 * TAU;
        for (let s = 1; s <= 10; s++) { const p = this.proj(j.p[0] + Math.cos(a) * R * .6 + Math.sin(j.ph * 2 + s * .7 + t) * .02 * (1 + e * 2), j.p[1] - s * R * .22, j.p[2] + Math.sin(a) * R * .6); this.dot(g, p, 1.1, (.5 - s * .04) * (.5 + e)); }
      }
      j.sp = this.proj(...j.p);
    }
  }
  /* the HUD: a title rule, tags (white / blue boxes) joined to dashed rings round each species, a speed legend, and
     on a wide stage a column of panels (a heat map of the tracks, a readout, a signal trace, data noise) */
  drawHud(c, byte) {
    const d = this.dpr, W = this.w, H = this.h, I = this.info; c.save(); c.scale(d, d);
    const font = (n) => `${n}px ${byte ? '"VT323", ' : ""}"IBM Plex Mono", ui-monospace, monospace`;
    c.textBaseline = "middle"; c.lineWidth = 1;
    c.strokeStyle = "rgba(235,235,255,.5)";
    const title = `PARTICLE DYNAMICS  |  GEN RADIO  |  ${I.genre || "—"}`; c.font = font(11);
    const tw = c.measureText(title).width; c.fillStyle = "rgba(235,235,255,.8)"; c.fillText(title, (W - tw) / 2, 22);
    c.beginPath(); c.moveTo(Math.min(200, (W - tw) / 2 - 20), 22); c.lineTo((W - tw) / 2 - 14, 22); c.moveTo((W + tw) / 2 + 14, 22); c.lineTo(W - 24, 22); c.stroke();
    for (const [x, y, sx, sy] of [[10, 10, 1, 1], [W - 10, 10, -1, 1], [10, H - 10, 1, -1], [W - 10, H - 10, -1, -1]]) { c.beginPath(); c.moveTo(x, y + sy * 14); c.lineTo(x, y); c.lineTo(x + sx * 14, y); c.stroke(); }
    const anchors = [this.cells[0] && this.cells[0].sp, this.schoolC, this.fall[0] && this.proj(...this.fall[0].p), this.colonyC, this.jellies[0] && this.jellies[0].sp, this.proj(this.trunks[0].x, -.6, this.trunks[0].z)];
    const colX = this.wide ? Math.max(24, this.cx - this.S * 2.3) : 14;
    for (let i = 0; i < 6; i++) {
      const p = anchors[i]; if (!p) continue;
      const left = i % 2 === 0 || !this.wide, tx = left ? colX : this.cx + this.S * 1.6, ty = 74 + i * (this.wide ? (H - 300) / 5 : 26);
      const active = this.env[i] > .45, label = `${active ? "‡" : ""}T${i + 1} ${NAMES[i]}`;
      c.font = font(10); const lw = c.measureText(label).width + 10;
      c.setLineDash([2, 3]); c.strokeStyle = active ? "rgba(70,90,255,.9)" : "rgba(235,235,255,.45)";
      c.beginPath(); c.arc(p[0], p[1], 12 + this.hit[i] * 10 + this.env[i] * 8, 0, TAU); c.stroke();
      c.setLineDash([]); c.beginPath(); c.moveTo(left ? tx + lw : tx, ty); c.lineTo(p[0], p[1]); c.stroke();
      c.fillStyle = active ? "#2236ff" : "#f1f1f1"; c.fillRect(tx, ty - 7, lw, 14);
      c.fillStyle = active ? "#fff" : "#000"; c.fillText(label, tx + 5, ty + .5);
      c.fillStyle = TRACK_COLORS[i]; c.fillRect(left ? tx - 6 : tx + lw + 2, ty - 7, 4, 14);
      c.fillStyle = "rgba(9,12,40,.9)"; c.fillRect(p[0] - 3, p[1] - 3, 6, 6); c.strokeStyle = "#9fb0ff"; c.strokeRect(p[0] - 3, p[1] - 3, 6, 6);
    }
    if (!this.wide) { c.restore(); return; }
    const px = W - Math.min(320, W * .27) - 24, pw = W - px - 28;
    c.font = font(10);
    const gr = c.createLinearGradient(px, 0, px + pw, 0); gr.addColorStop(0, speedCol(0, 1)); gr.addColorStop(.5, speedCol(.5, 1)); gr.addColorStop(1, speedCol(1, 1));
    c.fillStyle = gr; c.fillRect(px, 56, pw, 5); c.fillStyle = "rgba(235,235,255,.75)"; c.fillText("QUIET", px, 48); c.fillText("LOUD", px + pw - c.measureText("LOUD").width, 48);
    const vlabel = (txt, y, h) => { c.save(); c.translate(px - 10, y + h / 2); c.rotate(-Math.PI / 2); c.fillStyle = "rgba(235,235,255,.6)"; c.fillText(txt, -c.measureText(txt).width / 2, 0); c.restore(); };
    let y = 78; const hh = 6 * 11;                                // the heat map: each track's level over time
    c.strokeStyle = "rgba(235,235,255,.35)"; c.strokeRect(px - .5, y - .5, pw + 1, hh + 1); vlabel("TRACK MAP", y, hh);
    const cw = pw / 64;
    for (let i = 0; i < 6; i++) for (let k = 0; k < 64; k++) { const v = this.hist[i][k]; if (v < .02) continue; c.fillStyle = jet(v); c.fillRect(px + k * cw, y + i * 11 + 1, Math.ceil(cw), 9); }
    c.fillStyle = "rgba(235,235,255,.55)"; for (let i = 0; i < 6; i++) c.fillText(`T${i + 1}`, px + pw + 4, y + i * 11 + 6);
    y += hh + 26;                                                 // the readout
    c.fillStyle = "#e9f25a"; c.font = `${byte ? 22 : 18}px ${byte ? '"Silkscreen", ' : ""}"Michroma", "IBM Plex Mono", monospace`; c.shadowColor = "#e9f25a"; c.shadowBlur = 10;
    const tot = this.env.reduce((a, b) => a + b, 0) / 6;
    c.fillText(`SEED ${I.seed ?? "—"}`, px, y); c.shadowBlur = 0;
    c.font = font(10); c.fillStyle = "rgba(233,242,90,.8)";
    c.fillText(`MOOD ${I.mood > 0 ? "+" : ""}${I.mood ?? 0} · ${I.form || "—"} · E ${tot.toFixed(2)}`, px, y + 20);
    for (let i = 0; i < 6; i++) { c.fillStyle = "#e9f25a"; c.fillRect(px + i * 16, y + 32, 12, 4 + this.env[i] * 10); }
    y += 62;                                                      // the signal: a double helix of the whole level
    c.strokeStyle = "rgba(235,235,255,.35)"; c.strokeRect(px - .5, y - .5, pw + 1, 44); vlabel("SIGNAL", y, 44);
    for (const ph of [0, Math.PI]) { c.strokeStyle = "#3fe0e6"; c.lineWidth = 1.2; c.beginPath();
      for (let x = 0; x <= pw; x += 3) { const yy = y + 22 + Math.sin(x * .07 + this.t * 4 + ph) * (4 + tot * 16) * Math.sin(x / pw * Math.PI); x ? c.lineTo(px + x, yy) : c.moveTo(px + x, yy); } c.stroke(); }
    c.lineWidth = 1; y += 60;                                     // data noise, denser as it plays
    const nh = Math.min(70, H - y - 150); if (nh > 20) {
      c.strokeStyle = "rgba(235,235,255,.35)"; c.strokeRect(px - .5, y - .5, pw + 1, nh + 1); vlabel("DATA", y, nh);
      const cols = 60, rows = Math.floor(nh / 4), cwn = pw / cols, th = .55 - tot * .4;
      for (let r = 0; r < rows; r++) for (let k = 0; k < cols; k++) { if (Math.random() < .08 || ((r * 7 + k * 13 + (this.t * 6 | 0)) % 17) / 17 > th) { c.fillStyle = Math.random() < .03 ? "#2236ff" : "#d8d8d8"; c.fillRect(px + k * cwn, y + r * 4, Math.ceil(cwn), 3); } }
    }
    c.restore();
  }
}
/* the stems' planes (Float32Array per track) -> levels 0..1, each track against its own recent loudest */
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
