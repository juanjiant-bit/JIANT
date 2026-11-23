// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// JIANT Studio's recorder: every input channel as it comes (up to 8: the FM-1's USB > STEMS), in chunks of ~0.1 s
// posted to the page with their peak; "flush" posts what is left.
class StudioRecorder extends AudioWorkletProcessor {
  constructor() {
    super();
    this.buf = null; this.n = 0; this.peak = 0;
    this.port.onmessage = (e) => { if (e.data === "flush") this.flush(); };
  }
  flush() {
    if (!this.buf || !this.n) return;
    const chunk = this.buf.map((p) => p.slice(0, this.n));
    this.port.postMessage({ chunk, peak: this.peak }, chunk.map((p) => p.buffer));
    this.buf = null; this.n = 0; this.peak = 0;
  }
  process(inputs) {
    const ins = inputs[0];
    if (!ins || !ins.length) return true;
    if (!this.buf) this.buf = ins.map(() => new Float32Array(4096));
    for (let c = 0; c < ins.length && c < this.buf.length; c++) {
      const p = ins[c];
      this.buf[c].set(p, this.n);
      for (let i = 0; i < p.length; i += 8) { const v = Math.abs(p[i]); if (v > this.peak) this.peak = v; }
    }
    this.n += ins[0].length;
    if (this.n + 128 > 4096) this.flush();
    return true;
  }
}
registerProcessor("studio-recorder", StudioRecorder);
