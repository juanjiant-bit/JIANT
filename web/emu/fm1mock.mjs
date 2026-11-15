// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// (emu-transfer) A simulated FM-1 behind a simulated Web MIDI access, for transfer_test.mjs: SysEx in (F0 7D 46 4C cmd
// args F7), one reply out, asynchronously, as a USB-MIDI port delivers it. Two devices:
//   jsDevice    the backup protocol as web/EDITOR_PROTOCOL.md "v7: full backup" says (LIST snapshot, GET in pieces,
//               PUT begin / data / commit / abort, the rc codes, stale after a begin), INFO with its tags, and the
//               faults a test asks for: an older firmware (its version, its object count, its project size, no
//               JIANT tag), a cable pulled after n PUT pieces, a reply lost
//   wasmDevice  the firmware itself (felucca.wasm, transfer-api.h: editor_backup.c) answering, INFO made up from its
//               version as the device's would start
import { BACKUP_IDS, BACKUP_IDS_8, bkU32, bkR32, bkPack, bkUnpack, bkCrc } from "../fm1backup.js";

/* an INFO reply: version, NENGINES, P_COUNT, G_COUNT, NSTEP, P_E0, the engines' names, NTRK, CHAIN_ROWS, the tags */
export function infoReply({ version = "JIANT V10.7.36", jiant = true, backupCaps = 3, engines = ["ANALOG", "-", "PHASE"] } = {}) {
  const s = (t) => [...t].map((c) => c.charCodeAt(0) & 127).concat(0);
  return [...s(version), engines.length, 99, 40, 64, 91, ...engines.flatMap(s), 6, 16,
          0x55, 1, 3, 0x4D, 1, 64, 1, 0x42, 1, backupCaps, ...(jiant ? [0x4A, 1, 1] : []), 0x43, 2, 1, 3];
}

export function jsDevice(objs = new Map(), opt = {}) {
  const d = { objs: new Map(objs), log: [], pieces: 0, snap: null, stage: null, dead: false, opt };
  const ids = () => opt.ids || BACKUP_IDS;
  d.handle = (cmd, a) => {
    if (cmd === 1) return infoReply(opt.info || {});
    if (cmd === 65) {                                      // LIST: the snapshot (id 0 as it is now)
      d.snap = new Map(ids().map((id) => [id, (d.objs.get(id) || new Uint8Array(0)).slice()])); d.stage = null;
      const out = [1, 0, ids().length];
      for (const id of ids()) { const v = d.snap.get(id); out.push(id, ...bkU32(v.length), ...bkU32(bkCrc(v))); }
      return out;
    }
    if (cmd === 66) {
      const id = a[0], off = bkR32(a, 1), n = a[6] | a[7] << 7;
      if (!d.snap) return [id, 5, ...bkU32(off), 0, 0];
      const v = id === 0 ? d.snap.get(0) : d.objs.get(id) || new Uint8Array(0);
      if (!ids().includes(id) || !n || n > 256 || off + n > v.length) return [id, 1, ...bkU32(off), 0, 0];
      return [id, 0, ...bkU32(off), n & 127, n >> 7, ...bkPack(v.subarray(off, off + n))];
    }
    if (cmd === 67) {
      const [op, id] = a;
      if (op === 0) {
        d.snap = null;                                     // (a begin ends the snapshot: a GET now gets rc 5)
        if (!ids().includes(id) || id === 8 && opt.refuse8) { d.log.push(`refused ${id}`); return [op, id, 1]; }
        const size = bkR32(a, 2);
        if (opt.maxProject && id >= 40 && size > opt.maxProject) { d.log.push(`refused ${id}`); return [op, id, 1]; }
        d.stage = { id, size, crc: bkR32(a, 7), bytes: [] };
        return [op, id, 0];
      }
      if (!d.stage || d.stage.id !== id) return [op, id, 5];
      if (op === 1) {
        if (bkR32(a, 2) !== d.stage.bytes.length) return [op, id, 1];
        const n = Math.min(256, d.stage.size - d.stage.bytes.length);
        d.stage.bytes.push(...bkUnpack(a.slice(7), n));
        d.pieces++;
        return [op, id, 0];
      }
      if (op === 2) {
        const st = d.stage; d.stage = null;
        if (!commitOk(st)) return [op, id, 2];
        d.objs.set(id, Uint8Array.from(st.bytes)); d.log.push(id);
        return [op, id, 0];
      }
      if (op === 3) { d.stage = null; d.log.push(`abort ${id}`); return [op, id, 0]; }
    }
    return null;
  };
  return d;
}
/* the commit's own CRC check (the stage's CRC against what arrived): a test may corrupt a piece in flight */
const commitOk = (st) => bkCrc(Uint8Array.from(st.bytes)) === st.crc && st.bytes.length === st.size;

export function wasmDevice(ex, edServe, version) {
  return { handle: (cmd, a) => cmd === 1 ? infoReply({ version }) : edServe(ex, [cmd, ...a]), log: [], objs: null };
}

/* a Web MIDI access with one in/out pair named `name` around device d. opt.pullAfter: the cable pulled after that many
 * PUT data pieces (both ports "disconnected", nothing answered); opt.loseReply(cmd, args): that reply never arrives */
export function midiAccess(d, { name = "FM-1", manufacturer = "M-VAVE", pullAfter = Infinity, loseReply = null, delay = 0 } = {}) {
  const input = { name, manufacturer, state: "connected", onmidimessage: null, open: async () => {} };
  const output = { name, manufacturer, state: "connected", open: async () => {} };
  let pieces = 0;
  pullAfter ??= Infinity;
  output.send = (bytes) => {
    if (output.state === "disconnected") throw new DOMException("disconnected", "InvalidStateError");
    const b = Array.from(bytes);
    if (b[0] !== 0xF0 || b[1] !== 0x7D || b[2] !== 0x46 || b[3] !== 0x4C || b.at(-1) !== 0xF7) return;
    const cmd = b[4], args = b.slice(5, -1);
    if (cmd === 67 && args[0] === 1 && ++pieces > pullAfter) { input.state = output.state = "disconnected"; return; }
    if (loseReply && loseReply(cmd, args)) return;
    const r = d.handle(cmd, args);
    if (r) reply([cmd, ...r]);
  };
  const reply = ([cmd, ...body]) => setTimeout(() => {
    if (input.state !== "disconnected") input.onmidimessage?.({ data: Uint8Array.from([0xF0, 0x7D, 0x46, 0x4C, cmd, ...body, 0xF7]) });
  }, delay);
  const access = { inputs: new Map([["in", input]]), outputs: new Map([["out", output]]), input, output,
                   plug() { input.state = output.state = "connected"; pieces = -Infinity; } };
  return access;
}
export { BACKUP_IDS_8 };
