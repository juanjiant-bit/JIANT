// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// JIANT Studio's tracks view, as a DAW's: a lane for each channel that comes in (its name, its track's colour, a level
// meter, mute and solo), the wave drawn as it records, then the whole take with a playhead, played back with each
// lane's mute and solo. No DOM outside the element it is given; studio.js feeds it.

const PX_PER_S = 40;                       // the time scale while recording and of a take shown whole (fits the width)
export class Daw {
  constructor(root, labels) {
    this.root = root; this.labels = labels || {};
    this.lanes = []; this.secs = 0; this.rate = 44100; this.planes = null;
    this.ac = null; this.srcs = []; this.gains = []; this.t0 = 0; this.raf = 0; this.playing = false;
    this.onState = () => {};
  }
  /* lanes: [{name, sub, color, pan}] (pan: -1 left, 1 right, 0 centre) */
  setLanes(lanes) {
    this.stop();
    this.planes = null; this.secs = 0;
    this.root.replaceChildren();
    this.lanes = lanes.map((l, i) => {
      const row = document.createElement("div"); row.className = "lane"; row.style.setProperty("--lc", l.color);
      const head = document.createElement("div"); head.className = "lhead";
      const name = document.createElement("div"); name.className = "lname"; name.textContent = l.name;
      const sub = document.createElement("div"); sub.className = "lsub"; sub.textContent = l.sub || "";
      const btns = document.createElement("div"); btns.className = "lbtns";
      const m = document.createElement("button"); m.type = "button"; m.textContent = "M"; m.title = this.labels.mute || "Mute"; m.setAttribute("aria-pressed", "false");
      const s = document.createElement("button"); s.type = "button"; s.textContent = "S"; s.title = this.labels.solo || "Solo"; s.setAttribute("aria-pressed", "false");
      m.addEventListener("click", () => { lane.mute = !lane.mute; m.setAttribute("aria-pressed", String(lane.mute)); this.mix(); });
      s.addEventListener("click", () => { lane.solo = !lane.solo; s.setAttribute("aria-pressed", String(lane.solo)); this.mix(); });
      btns.append(m, s);
      const meter = document.createElement("div"); meter.className = "lmeter"; const fill = document.createElement("i"); meter.append(fill);
      head.append(name, sub, btns, meter);
      const wave = document.createElement("canvas"); wave.className = "lwave"; wave.height = 56;
      row.append(head, wave);
      this.root.append(row);
      const lane = { ...l, i, row, fill, wave, ctx: wave.getContext("2d"), x: 0, mute: false, solo: false, peak: 0, silent: true };
      return lane;
    });
    this.playhead = document.createElement("div"); this.playhead.className = "playhead"; this.playhead.hidden = true;
    this.root.append(this.playhead);
    this.fit();
  }
  fit() {
    for (const l of this.lanes) {
      const w = Math.max(200, Math.floor(l.wave.clientWidth || 600));
      if (l.wave.width !== w) { l.wave.width = w; l.x = 0; l.ctx.clearRect(0, 0, w, l.wave.height); }
    }
  }
  /* live: the levels (0..1 a channel) and a chunk of planes (each a Float32Array) drawn at the right of each lane,
     scrolling left once it fills */
  live(planes, rate) {
    this.rate = rate;
    const n = planes[0] ? planes[0].length : 0, cols = Math.max(1, Math.round(n / rate * PX_PER_S));
    this.lanes.forEach((l, k) => {
      const p = planes[k]; if (!p) return;
      let peak = 0;
      for (let i = 0; i < p.length; i += 4) { const v = Math.abs(p[i]); if (v > peak) peak = v; }
      l.peak = Math.max(l.peak * 0.8, peak); l.fill.style.width = `${Math.min(100, l.peak * 100).toFixed(0)}%`;
      if (peak > 0.002) l.silent = false;
      const w = l.wave.width, h = l.wave.height, c = l.ctx;
      if (l.x + cols > w) { const sh = l.x + cols - w; c.drawImage(l.wave, -sh, 0); c.clearRect(w - sh, 0, sh, h); l.x = w - cols; }
      c.fillStyle = l.color;
      const per = n / cols;
      for (let x = 0; x < cols; x++) {
        let lo = 0, hi = 0;
        for (let i = Math.floor(x * per), e = Math.floor((x + 1) * per); i < e; i++) { const v = p[i]; if (v < lo) lo = v; else if (v > hi) hi = v; }
        const y0 = (0.5 - Math.min(1, hi) * 0.5) * h, y1 = (0.5 - Math.max(-1, lo) * 0.5) * h;
        c.fillRect(l.x + x, y0, 1, Math.max(1, y1 - y0));
      }
      l.x += cols;
    });
  }
  meters(peaks) {
    this.lanes.forEach((l, k) => { const v = peaks[k] || 0; l.peak = Math.max(l.peak * 0.8, v); l.fill.style.width = `${Math.min(100, l.peak * 100).toFixed(0)}%`; if (v > 0.002) l.silent = false; });
  }
  /* a take shown whole: each lane's wave over the width, then playable */
  show(planes, rate) {
    this.planes = planes; this.rate = rate; this.secs = planes[0] ? planes[0].length / rate : 0;
    this.fit();
    this.lanes.forEach((l, k) => {
      const p = planes[k], w = l.wave.width, h = l.wave.height, c = l.ctx;
      c.clearRect(0, 0, w, h); if (!p || !p.length) return;
      c.fillStyle = l.color; const per = p.length / w;
      for (let x = 0; x < w; x++) {
        let lo = 0, hi = 0;
        for (let i = Math.floor(x * per), e = Math.floor((x + 1) * per); i < e; i++) { const v = p[i]; if (v < lo) lo = v; else if (v > hi) hi = v; }
        const y0 = (0.5 - Math.min(1, hi) * 0.5) * h, y1 = (0.5 - Math.max(-1, lo) * 0.5) * h;
        c.fillRect(x, y0, 1, Math.max(1, y1 - y0));
      }
      l.x = 0;
    });
  }
  mix() {
    const solo = this.lanes.some((l) => l.solo);
    this.lanes.forEach((l, k) => {
      l.row.classList.toggle("off", l.mute || (solo && !l.solo));
      if (this.gains[k]) this.gains[k].gain.value = l.mute || (solo && !l.solo) ? 0 : 1;
    });
  }
  async play() {
    if (!this.planes || this.playing) return;
    this.ac = this.ac || new AudioContext();
    await this.ac.resume();
    const n = this.planes[0].length;
    this.srcs = []; this.gains = [];
    this.planes.forEach((p, k) => {
      const l = this.lanes[k]; if (!l) return;
      const b = this.ac.createBuffer(1, n, this.rate); b.copyToChannel(p, 0);
      const s = this.ac.createBufferSource(); s.buffer = b;
      const g = this.ac.createGain(), pan = this.ac.createStereoPanner(); pan.pan.value = l.pan || 0;
      s.connect(g).connect(pan).connect(this.ac.destination);
      this.srcs.push(s); this.gains[k] = g;
    });
    this.mix();
    this.t0 = this.ac.currentTime + 0.05;
    this.srcs.forEach((s) => s.start(this.t0));
    this.srcs[0].onended = () => this.stop();
    this.playing = true; this.playhead.hidden = false; this.onState(true);
    const step = () => {
      if (!this.playing) return;
      const f = Math.min(1, (this.ac.currentTime - this.t0) / this.secs), l = this.lanes[0];
      if (l) { const r = l.wave.getBoundingClientRect(), root = this.root.getBoundingClientRect(); this.playhead.style.left = `${r.left - root.left + f * r.width}px`; }
      this.raf = requestAnimationFrame(step);
    };
    step();
  }
  stop() {
    if (!this.playing) return;
    this.playing = false; cancelAnimationFrame(this.raf);
    this.srcs.forEach((s) => { try { s.onended = null; s.stop(); } catch (_) {} });
    this.srcs = []; this.gains = []; this.playhead.hidden = true; this.onState(false);
  }
  /* which lanes stayed silent (no signal while it recorded) */
  silentLanes() { return this.lanes.filter((l) => l.silent).map((l) => l.i); }
}
