// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
// Felucca in an AudioWorklet: felucca.wasm is the whole FM-1 firmware against a simulated FM-1
// (web/emu/felucca_web.c). Each render quantum asks it for 128 frames, which runs the device's clock forward;
// between quanta the page's input goes in and, about 30 times a second (4 when the page is hidden), the screen,
// the lights and any changed flash go out. After X0X's web/emu/worklet.js (charlesvestal/fm1-x0x, GPL-3.0).

// The flash the page keeps: storage.c's areas (projects, user presets and their FM6 patches, the songs
// 0x97000..0xDFFFF; the autosave 0xE5000..0xE6FFF (1.2); the settings and the second FM6 copy
// 0xFC000..0xFFFFF), only the sectors that hold something (an erased sector is all 0xFF). {offset: bytes}
export const STORE_AREAS = [[0x97000, 0xE0000], [0xE5000, 0xE7000], [0xFC000, 0x100000]];
export function storeSectors(nor) {
  const out = {};
  for (const [a, b] of STORE_AREAS)
    for (let off = a; off < b; off += 4096) {
      const s = nor.subarray(off, off + 4096);
      if (s.some((v) => v !== 0xFF)) out[off] = s.slice();
    }
  return out;
}
export function restoreSectors(nor, sectors) {
  for (const [k, v] of Object.entries(sectors || {})) {
    const off = +k, b = v instanceof Uint8Array ? v : new Uint8Array(v);
    if (STORE_AREAS.some(([a, e]) => off >= a && off + 4096 <= e && off % 4096 === 0) && b.length === 4096)
      nor.set(b, off);
  }
}

export const HOLD_MS = 24;

/* (ports) felucca_hp.wasm, the big CPUs' build (web/emu/felucca_hp.c, devices.html's MPC Sample panel): its memory
 * grows with the samples (the one import it has; felucca.wasm has none and ignores it) */
export const WASM_IMPORTS = { env: { emscripten_notify_memory_growth() {} } };
/* the 16 sample slots as the page shows them: [{frames, ch, name, gen, changed}] */
export function slotList(ex) {
  const out = [];
  for (let k = 0; k < ex.web_slot_count(); k++) {
    const p = ex.web_slot_info(k), u = new Uint32Array(ex.memory.buffer, p, 12), b = new Uint8Array(ex.memory.buffer, p + 8, 32);
    const z = b.indexOf(0);
    out.push({ frames: u[0], ch: u[1], name: String.fromCharCode(...b.subarray(0, z < 0 ? 32 : z)), gen: u[10], changed: !!u[11] });
  }
  return out;
}
/* a sample into a slot: the page's float PCM (decoded at 44.1 kHz) copied into the device, made into a sample object
 * and handed to the slot (a pointer swap), what the slot held freed by web_service: between render quanta, never in one.
 * 0 done, 1 bad slot, 2 no memory */
export function slotLoad(ex, slot, name, l, r, rate) {
  const n = l.length, pl = ex.web_alloc(n * 4), pr = r ? ex.web_alloc(n * 4) : 0;
  if (!pl || (r && !pr)) { if (pl) ex.web_free(pl); if (pr) ex.web_free(pr); return 2; }
  new Float32Array(ex.memory.buffer, pl, n).set(l);
  if (pr) new Float32Array(ex.memory.buffer, pr, n).set(r.subarray(0, n));
  const nm = new Uint8Array(ex.memory.buffer, ex.web_name_buf(), 64);
  nm.fill(0);
  const s = String(name || "").replace(/[^\x20-\x7E]/g, "?").slice(0, 31);
  for (let i = 0; i < s.length; i++) nm[i] = s.charCodeAt(i);
  const o = ex.web_sample_make(pl, pr, n, rate | 0);
  ex.web_free(pl); if (pr) ex.web_free(pr);
  if (!o) return 2;
  const rc = ex.web_slot_put(slot >>> 0, o);
  if (rc) ex.web_sample_free(o);
  ex.web_service();
  return rc;
}
/* a slot's PCM as it plays (int16 planes, copied out), or null when empty */
export function slotRead(ex, slot) {
  const [info] = slotList(ex).slice(slot, slot + 1);
  if (!info || !info.frames) return null;
  const plane = (c) => new Int16Array(ex.memory.buffer, ex.web_slot_pcm(slot, c), info.frames).slice();
  return { ...info, l: plane(0), r: info.ch > 1 ? plane(1) : null };
}

/* (emu-transfer) the editor's backup commands into the emulator (web/emu/transfer-api.h: the firmware's
 * editor_backup.c): a request [cmd, ...args] (7-bit bytes) in, the device's reply [...] out, null when it sends none.
 * The page's transfer panel (transfer.js) talks to it over a MessagePort of its own ("edport"), so the page's
 * own messages stay as they are. A flash write goes out with the next frame's sectors, as a save from the panel does. */
export function edServe(ex, req) {
  if (!ex.web_ed || !Array.isArray(req) || !req.length || req.length > 400) return null;
  new Uint8Array(ex.memory.buffer, ex.web_ed_buf(), req.length).set(req.map((v) => v & 255));
  const n = ex.web_ed(req.length);
  return n ? Array.from(new Uint8Array(ex.memory.buffer, ex.web_ed_reply(), n)) : null;
}
/* (studio) the editor's replies and pushes waiting in the device (editor-api.h), each a whole F0..F7 message */
export function sysexOut(ex) {
  const out = [];
  for (let n, k = 0; k < 64 && (n = ex.web_sysex_out()); k++) out.push(Array.from(new Uint8Array(ex.memory.buffer, ex.web_sysex_msg(), n)));
  return out;
}
export function edVersion(ex) {
  if (!ex.web_version) return "";
  const m = new Uint8Array(ex.memory.buffer, ex.web_version(), 64), z = m.indexOf(0);
  return String.fromCharCode(...m.subarray(0, z < 0 ? 64 : z));
}

if (typeof registerProcessor === "function") {
  const clock = globalThis.performance ? () => globalThis.performance.now() : () => Date.now();
  class Felucca extends AudioWorkletProcessor {
    constructor() {
      super();
      this.ex = null;
      this.sentDraws = -1;
      this.sentWrites = -1;
      this.sentLeds = "";
      this.lastFrame = 0;
      this.period = 1 / 30;
      this.busy = 0;
      this.frames = 0;
      this.queue = [];                                // button / key changes, each held >= HOLD_MS of device time
      this.nextOk = 0;
      this.meterSum = this.meterN = this.meterPeak = 0;
      this.sentSlots = "";
      this.port.onmessage = async (e) => {
        try { await this.onMessage(e.data); }
        catch (error) {
          this.ex = null;
          this.port.postMessage({type:"error", message:String(error && error.message || error)});
        }
      };
    }

    async onMessage(m) {
      if (m.type === "load") {
        try {
          const { instance } = await WebAssembly.instantiate(m.wasm, WASM_IMPORTS);
          const ex = instance.exports;
          ex._initialize();
          ex.web_nor_erase();
          restoreSectors(new Uint8Array(ex.memory.buffer, ex.web_nor(), ex.web_nor_size()), m.sectors);
          ex.web_master(m.master ?? 700);
          if (ex.web_seed) ex.web_seed((Math.random() * 4294967296) >>> 0);   // (JIANT 0.4) this session's macros
          ex.web_boot();
          if (ex.web_service) ex.web_service();      // (ports) the recorder's first object
          const samples = ex.web_sample_count ? ex.web_sample_count() : 0;
          const kitReady = !!(samples && m.sampleKit && ex.web_sample_kit && !ex.web_sample_kit());
          this.ex = ex;
          this.sentWrites = ex.web_flash_writes_count();
          this.port.postMessage({ type: "ready", rate: ex.web_sample_rate(), samples, kitReady, additive: !!(ex.web_additive_edition && ex.web_additive_edition()), hostPower: !!ex.web_host_power });
        } catch (err) {
          this.port.postMessage({ type: "error", message: String(err && err.message || err) });
        }
        return;
      }
      const ex = this.ex;
      if (!ex) return;
      if (m.type === "sample-kit") {
        const samples = ex.web_sample_count ? ex.web_sample_count() : 0;
        const kitReady = !!(samples && ex.web_sample_kit && !ex.web_sample_kit());
        this.port.postMessage({type:"sample-ready", samples, kitReady});
      }
      else if (m.type === "diagnostics") this.port.postMessage({type:"diagnostics", additive:!!(ex.web_additive_edition && ex.web_additive_edition()), engine:ex.web_selected_engine ? ex.web_selected_engine() : null});
      else if (m.type === "buttons" || m.type === "keys") this.queue.push(m);
      else if (m.type === "enc") ex.web_enc(m.role, m.n | 0);
      else if (m.type === "master") ex.web_master(m.value | 0);
      else if (m.type === "midi") ex.web_midi(m.data[0] | 0, m.data[1] | 0, m.data[2] | 0);
      else if (m.type === "tap") {                 // (studio) the stems and MIDI OUT to JIANT Studio's recorder
        const on = !!m.on && !!ex.web_stems;
        if (ex.web_stems) ex.web_stems(on ? 1 : 0);
        if (ex.web_midi_tap) ex.web_midi_tap(on ? 1 : 0);
        this.tap = on ? { planes: Array.from({ length: 8 }, () => new Float32Array(4096)), n: 0, at: this.pos, midi: [] } : null;
        this.port.postMessage({ type: "tap", on, stems: !!ex.web_stems });
      }
      else if (m.type === "sysex") {               // (studio) the web editor's frame (editor-api.h): its replies at once
        if (!ex.web_sysex || !Array.isArray(m.data) || m.data.length > 640) return;
        new Uint8Array(ex.memory.buffer, ex.web_sysex_buf(), m.data.length).set(m.data.map((v) => v & 255));
        ex.web_sysex(m.data.length);
        const out = sysexOut(ex);
        if (out.length) this.port.postMessage({ type: "sysex", msgs: out });
      }
      else if (m.type === "gen") {                 // (studio RADIO) the generator: a command (web_gen_cmd), then its state
        let rc = null;                             // 10 RESOUND (arg: a seed 0..9999), 11 its undo; else web_gen_cmd
        if (m.cmd === 10) rc = ex.web_gen_resound ? ex.web_gen_resound(m.arg >>> 0) : null;
        else if (m.cmd === 11) rc = ex.web_gen_resound_undo ? ex.web_gen_resound_undo() : null;
        else if (m.cmd != null && ex.web_gen_cmd) ex.web_gen_cmd(m.cmd >>> 0, m.arg | 0);
        let info = null, genres = null;
        if (ex.web_gen_info) {
          const p = ex.web_gen_info();                // a static int32[8] of the firmware's
          if (p) info = Array.from(new Int32Array(ex.memory.buffer, p, 8));
        }
        if (m.genres && ex.web_gen_genre) {
          genres = [];
          for (let i = 0, a; i < 64 && (a = ex.web_gen_genre(i)); i++) {
            const b = new Uint8Array(ex.memory.buffer, a, 32), z = b.indexOf(0);
            genres.push(String.fromCharCode(...b.subarray(0, z < 0 ? 32 : z)));
          }
        }
        const rs = ex.web_gen_resound_seed ? ex.web_gen_resound_seed() >>> 0 : 0xFFFFFFFF;
        this.port.postMessage({ type: "gen", id: m.id, ok: !!ex.web_gen_info, info, genres, rc, resound: !!ex.web_gen_resound, rs: rs === 0xFFFFFFFF ? null : rs });
      }
      else if (m.type === "visible") this.period = m.on ? 1 / 30 : 1 / 4;
      else if (m.type === "meter") {               // the output's level since the last ask (tests, the page)
        this.metering = true;
        this.port.postMessage({ type: "meter", id: m.id, rms: this.meterN ? Math.sqrt(this.meterSum / this.meterN) : 0, peak: this.meterPeak, frames: this.meterN });
        this.meterSum = this.meterN = this.meterPeak = 0;
      }
      else if (ex.web_host_power && this.samples(m)) { /* (ports) the sample slots' messages */ }
      else if (m.type === "edport" && m.port) {        // (emu-transfer) the transfer panel's channel
        const port = m.port;
        port.onmessage = (e) => {
          const q = e.data || {}, x = this.ex;
          let reply = null, error = "";
          try { reply = x ? edServe(x, q.req) : null; } catch (err) { error = String(err && err.message || err); }
          port.postMessage({ seq: q.seq, reply, error, version: x ? edVersion(x) : "" });
          if (x) this.sentDraws = -1;                   // (the screen after a restore, at once)
        };
        port.postMessage({ seq: 0, reply: null, version: edVersion(ex) });
      }
    }

    // (ports, felucca_hp.wasm) the sample slots, the recorder, the input: between render quanta, so a load's copy and
    // its allocation never run inside the audio callback (web_service frees what the slots let go of)
    samples(m) {
      const ex = this.ex;
      if (m.type === "slot-load") {
        const rc = slotLoad(ex, m.slot, m.name, m.l, m.r, m.rate || 44100);
        this.port.postMessage({ type: "slot-loaded", slot: m.slot, id: m.id, rc });
      } else if (m.type === "slot-clear") { ex.web_slot_clear(m.slot >>> 0); ex.web_service(); }
      else if (m.type === "slot-read") {
        const d = slotRead(ex, m.slot >>> 0);
        this.port.postMessage({ type: "slot-pcm", slot: m.slot, id: m.id, pcm: d }, d ? [d.l.buffer].concat(d.r ? [d.r.buffer] : []) : []);
      } else if (m.type === "slot-saved") ex.web_slot_saved(m.slot >>> 0, m.gen >>> 0);
      else if (m.type === "rec-start") {
        ex.web_service();
        this.port.postMessage({ type: "rec-started", id: m.id, rc: ex.web_rec_start(m.slot >>> 0, m.source >>> 0, m.bars >>> 0) });
      } else if (m.type === "rec-stop") { ex.web_rec_stop(); ex.web_service(); }
      else if (m.type === "service") ex.web_service();
      else if (m.type === "test-engine") ex.web_test_engine(m.track >>> 0, m.engine >>> 0);
      else if (m.type === "test-preset") ex.web_test_preset(m.track >>> 0, m.preset >>> 0);
      else return false;
      this.sentSlots = "";
      return true;
    }

    publish() {
      const ex = this.ex, mem = ex.memory.buffer;
      const msg = { type: "frame" };
      const leds = [ex.web_lit_buttons(), ex.web_lit_keys(), ex.web_dim_buttons(), ex.web_dim_keys(), ex.web_dim_level(),
                    ex.web_breath_buttons(), ex.web_breath_keys(), ex.web_mid_keys ? ex.web_mid_keys() : 0];
      const an = ex.web_anim_levels ? ex.web_anim_levels() : 0;   // the power-on sweep: each LED's level
      const anim = an ? Array.from(new Uint8Array(mem, an, 41)) : null;
      const key = leds.join(",") + (anim ? ";" + anim.join(",") : "");
      if (key !== this.sentLeds) {
        this.sentLeds = key;
        msg.leds = leds;
        msg.anim = anim;
      }
      if (this.frames >= 44100) {                     // the share of real time spent running the device
        msg.load = this.busy / (this.frames / 44.1);
        this.busy = this.frames = 0;
      }
      const transfer = [];
      const draws = ex.web_screen_draws();
      if (draws !== this.sentDraws) {
        this.sentDraws = draws;
        msg.fb = new Uint16Array(mem, ex.web_screen(), 240 * 240).slice();
        transfer.push(msg.fb.buffer);
      }
      const writes = ex.web_flash_writes_count();
      if (writes !== this.sentWrites) {               // a save is done within one main-loop pass: whole here
        this.sentWrites = writes;
        msg.sectors = storeSectors(new Uint8Array(mem, ex.web_nor(), ex.web_nor_size()));
      }
      if (ex.web_host_power) {                        // (ports) the slots and the recorder, when they change
        const slots = slotList(ex), rec = { st: ex.web_rec_state(), slot: ex.web_rec_slot(), frames: ex.web_rec_frames() };
        const k = JSON.stringify(slots) + JSON.stringify(rec);
        if (k !== this.sentSlots) { this.sentSlots = k; msg.slots = slots; msg.rec = rec; }
      }
      if (ex.web_editor_poll) {                       // (studio) the editor's live pushes (WATCH), in the same message
        ex.web_editor_poll();
        const out = sysexOut(ex);
        if (out.length) msg.sysex = out;
      }
      if (msg.leds || msg.fb || msg.sectors || msg.slots || msg.sysex || msg.load !== undefined) this.port.postMessage(msg, transfer);
    }

    // A click's down and up can arrive within one render quantum, but the FM-1's contacts never close for less than
    // its debounce, and the UI reads them once a frame: each change of the buttons or the keys stays HOLD_MS of the
    // device's time before the next one goes in (a tap: down for a frame and more, then up).
    input() {
      const ex = this.ex, now = ex.web_now_ms();
      while (this.queue.length && now - this.nextOk >= 0) {
        const m = this.queue.shift();
        if (m.type === "buttons") ex.web_buttons(m.mask >>> 0);
        else ex.web_keys(m.mask >>> 0);
        this.nextOk = now + HOLD_MS;
      }
    }

    /* (studio) a block's stems (web_stem_out: T1..T6, FX L, FX R) and the MIDI OUT packets it sent (each at its sample),
       posted about every 93 ms: { at: the first sample's number, planes, midi: [{ at, data }] } */
    tapBlock(ex, n) {
      const T = this.tap;
      for (let c = 0; c < 8; c++) T.planes[c].set(new Float32Array(ex.memory.buffer, ex.web_stem_out(c), n), T.n);
      for (let p; ex.web_midi_take && (p = ex.web_midi_take()) !== 0;) {
        const cin = p & 15, len = cin === 0xC || cin === 0xD ? 2 : cin >= 0x8 && cin <= 0xE ? 3 : cin === 0xF ? 1 : 0;
        if (len) T.midi.push({ at: this.pos, data: [(p >>> 8) & 255, (p >>> 16) & 255, (p >>> 24) & 255].slice(0, len) });
      }
      T.n += n;
      if (T.n + 128 > 4096) {
        const planes = T.planes.map((pl) => pl.slice(0, T.n));
        this.port.postMessage({ type: "tap-chunk", at: T.at, planes, midi: T.midi }, planes.map((pl) => pl.buffer));
        T.n = 0; T.at = this.pos + n; T.midi = [];
      }
    }

    process(inputs, outputs) {
      try { return this.renderBlock(inputs, outputs); }
      catch (error) {
        for (const channel of outputs[0] || []) channel.fill(0);
        this.ex = null;
        this.port.postMessage({type:"error", message:String(error && error.message || error)});
        return false;
      }
    }

    renderBlock(inputs, outputs) {
      const ex = this.ex;
      if (!ex) return true;
      if (this.queue.length) this.input();
      const out = outputs[0], n = out[0].length;
      const inp = inputs && inputs[0];
      if (ex.web_input && inp && inp.length) {         // (ports) the device's audio input (MIC / LINE), before the render
        new Float32Array(ex.memory.buffer, ex.web_input_l(), n).set(inp[0].subarray(0, n));
        new Float32Array(ex.memory.buffer, ex.web_input_r(), n).set((inp[1] || inp[0]).subarray(0, n));
        ex.web_input(n);
      }
      const t0 = clock();
      ex.web_render(n);
      if (this.tap) this.tapBlock(ex, n);
      this.pos = (this.pos || 0) + n;
      this.busy += clock() - t0;
      this.frames += n;
      out[0].set(new Float32Array(ex.memory.buffer, ex.web_out_l(), n));
      if (out[1]) out[1].set(new Float32Array(ex.memory.buffer, ex.web_out_r(), n));
      if (this.metering) for (let i = 0; i < n; i++) { const v = out[0][i]; this.meterSum += v * v; if (v > this.meterPeak) this.meterPeak = v; else if (-v > this.meterPeak) this.meterPeak = -v; }
      if (this.metering) this.meterN += n;
      if (currentTime - this.lastFrame >= this.period) {
        this.lastFrame = currentTime;
        this.publish();
      }
      return true;
    }
  }
  registerProcessor("felucca", Felucca);
}


