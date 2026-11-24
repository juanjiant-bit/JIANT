// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// JIANT Studio in Chromium (node web/studio/browser_test.mjs; Playwright): dev_server.py on a free port, then
// - the simulated FM-1 (?mock=1) found and named, the look by its firmware (MATH: JIANT BYTE);
// - the screen: SCREEN on, frames decoded and painted, full screen square in portrait and landscape;
// - install safety: an installer's "busy" on jiant-midi (from another tab): Studio answers "released", lets the FM-1 go,
//   stops the screen and refuses to find it again until the installer says it has finished;
// - the emulator's panel whole in its frame, and full screen; the other devices' shots and their manuals;
// - target Emulator (when build/emu holds an emulator with the editor, web/emu/build.sh): the emulator switched on in its
//   frame, the editor beside it connects to it and a preset picked there changes the emulator's screen; its output recorded.
import { spawn } from "node:child_process";
import { createRequire } from "node:module";
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";

const require = createRequire(import.meta.url);
const { chromium } = require("playwright");
const HERE = new URL(".", import.meta.url).pathname, ROOT = join(HERE, "..", "..");
let failed = 0;
const ok = (c, what) => { console.log(`studio browser: ${what.padEnd(70)} ${c ? "ok" : "FAIL"}`); if (!c) failed++; };
const PORT = 8790 + Math.floor(Math.random() * 100);
const server = spawn("python3", [join(HERE, "dev_server.py"), String(PORT)], { stdio: "ignore" });
const wait = (ms) => new Promise((r) => setTimeout(r, ms));
await wait(800);
const URL0 = `http://localhost:${PORT}/JIANT/studio/`;
const b = await chromium.launch({ args: ["--autoplay-policy=no-user-gesture-required"] });
try {
  const errs = [];
  const page = async (q, vp = { width: 1280, height: 860 }) => {
    const p = await (await b.newContext({ viewport: vp })).newPage();
    p.on("pageerror", (e) => errs.push(e.message));
    await p.goto(URL0 + q); await p.waitForTimeout(1500);
    return p;
  };
  /* the simulated FM-1 */
  const p = await page("?mock=1");
  ok((await p.textContent("#devtext")).startsWith("JIANT FM V10.8.24") && await p.evaluate(() => document.documentElement.dataset.skin) === "fm",
    "the simulated FM-1 found: JIANT FM, its version, its look");
  const pb = await page("?mock=1&fw=byte");
  ok((await pb.textContent("#devtext")).startsWith("JIANT BYTE") && await pb.evaluate(() => document.documentElement.dataset.skin) === "byte",
    "a JIANT BYTE (its engines include MATH): named, the BYTE look");
  /* firmware from before the mirror (JIANT BYTE v0.16: no reply to SCREEN): named once; Show tries, then says why */
  const old = await page("?mock=1&fw=byte&nomirror=1");
  await old.waitForTimeout(2500);
  await old.click('nav.views [data-go="screen"]'); await old.waitForTimeout(300);
  await old.click("#scrgo"); await old.waitForTimeout(1800);
  const o = await old.evaluate(() => ({ chip: document.getElementById("devtext").textContent, stat: document.getElementById("scrstat").textContent,
    on: jiantStudio.scr.on, fs: !!document.fullscreenElement }));
  ok(o.chip === "JIANT BYTE v0.16" && !o.on && !o.fs && /espejo|mirror/i.test(o.stat), `firmware without the mirror: "${o.chip}", Show tries and says why, no black full screen`);
  /* the screen */
  await p.click('nav.views [data-go="screen"]'); await p.click("#scrgo"); await p.waitForTimeout(1500);
  const lit = await p.evaluate(() => { const d = document.getElementById("scr").getContext("2d").getImageData(0, 0, 240, 240).data; let n = 0; for (let i = 0; i < d.length; i += 4) n += d[i] | d[i + 1] | d[i + 2] ? 1 : 0; return n; });
  ok(lit > 20000 && /KB\/s/.test(await p.textContent("#scrstat")) && await p.evaluate(() => document.getElementById("scrwait").hidden),
    `the FM-1's screen painted (${lit} pixels lit), its rate shown, the waiting line gone`);
  for (const [vp, name] of [[{ width: 412, height: 915 }, "portrait"], [{ width: 915, height: 412 }, "landscape"]]) {
    const m = await page("?mock=1&screen=1", vp);
    await m.waitForTimeout(800); await m.click("#scrfull"); await m.waitForTimeout(600);
    const r = await m.evaluate(() => { const c = document.getElementById("scr").getBoundingClientRect(); return { fs: document.fullscreenElement && document.fullscreenElement.id, w: c.width, h: c.height }; });
    ok(r.fs === "stage" && Math.abs(r.w - r.h) < 2 && Math.round(r.w) === Math.min(vp.width, vp.height), `full screen, ${name}: square, ${Math.round(r.w)} px (the short side)`);
  }
  /* record: the simulated FM-1 in STEMS+IN (10 channels): a lane each, named by track and engine; a take of each stem */
  {
    const r = await (await b.newContext({ viewport: { width: 1280, height: 900 } })).newPage();
    r.on("pageerror", (e) => errs.push(e.message));
    await r.goto(URL0 + "?mock=1#rec"); await r.waitForTimeout(1500);
    await r.click('nav.views [data-go="rec"]'); await r.waitForTimeout(600);
    await r.selectOption("#recdev", "mock-stems"); await r.click("#recmon"); await r.waitForTimeout(1200);
    const lanes = await r.$$eval(".lname", (x) => x.map((e) => e.textContent));
    await r.click("#recgo"); await r.waitForTimeout(2500); await r.click("#recgo"); await r.waitForTimeout(800);
    const files = await r.evaluate(() => jiantStudio.rec.takes[0].files.map((f) => f.label));
    ok(lanes.length === 10 && lanes[0] === "T1 · DRUM" && lanes[8] === "VOX" && lanes[9] === "DRY" &&
       ["T1", "T6", "FX", "VOX", "DRY"].every((l) => files.some((f) => f.startsWith(l))) && files.length >= 11 && !lanes.some((l) => / · -$/.test(l)),
      `record, STEMS+IN: 10 lanes (${lanes[0]} … ${lanes[9]}), a WAV a stem, the ZIP and the mix`);
  }
  /* the emulator's whole panel in its frame, and full screen (the panel refitted to it) */
  {
    const e = await page("?target=emu#emu", { width: 1280, height: 800 });
    await e.waitForTimeout(1500);
    const fit = () => e.evaluate(() => { const f = document.getElementById("f-emu"), d = f.contentDocument.getElementById("device");
      return d ? { bottom: d.getBoundingClientRect().bottom, frame: f.clientHeight, w: d.getBoundingClientRect().width } : null; });
    const a = await fit();
    await e.click("#emufull"); await e.waitForTimeout(900);
    const fsOn = await e.evaluate(() => (document.fullscreenElement && document.fullscreenElement.id) || (document.getElementById("emubox").classList.contains("max") ? "emubox" : ""));
    const z = await fit();
    ok(a && a.bottom <= a.frame + 1 && fsOn === "emubox" && z.bottom <= z.frame + 1 && z.w > a.w,
      `the emulator whole in its frame (${a && Math.round(a.bottom)} of ${a && a.frame} px), full screen larger (${z && Math.round(z.w)} px wide)`);
  }
  /* other devices: each port's shots (a tap: the dialog), its manual in HTML */
  {
    const d = await page("#ports", { width: 1280, height: 900 });
    await d.click('nav.views [data-go="ports"]'); await d.waitForTimeout(1500);
    const r = await d.evaluate(() => ({ imgs: [...document.querySelectorAll("#portlist .pic img")].map((i) => i.naturalWidth),
      thumbs: document.querySelectorAll("#portlist .thumbs button").length,
      manuals: [...document.querySelectorAll("#portlist a")].filter((a) => a.href.includes("/docs/guide/")).length }));
    await d.click("#portlist .picopen"); await d.waitForTimeout(400);
    const open = await d.evaluate(() => document.getElementById("shotdlg").open && document.getElementById("shotimg").naturalWidth > 0);
    ok(r.imgs.length === 3 && r.imgs.every((w) => w > 0) && r.thumbs === 3 && r.manuals === 5 && open,
      `other devices: ${r.imgs.length} ports with their shots, the plugin's 3, ${r.manuals} manuals in HTML, a shot opens large`);
  }
  /* projects and backups, by hand, on the simulated FM-1: the slots read, one saved; a backup kept in the browser,
     its summary, restored */
  {
    const q = await page("?mock=1#proj", { width: 1280, height: 900 });
    q.on("dialog", (d) => d.accept());
    await q.waitForTimeout(800);
    await q.click('nav.views [data-go="proj"]'); await q.waitForTimeout(400);
    await q.click("#slotsread"); await q.waitForTimeout(800);
    const before = await q.$$eval("#slots .slot .st", (x) => x.map((e) => e.textContent));
    await q.click("#slots .slot:nth-child(2) .btn.ghost"); await q.waitForTimeout(1500);
    const afterB = await q.$eval("#slots .slot:nth-child(2) .st", (e) => e.textContent);
    await q.uncheck("#bkdl"); await q.click("#bkgo"); await q.waitForTimeout(4000);
    const rows = await q.$$eval("#bklist .bkrow", (x) => x.map((e) => e.querySelector(".what").textContent));
    await q.click("#bklist .bkrow .acts .btn:not(.ghost)"); await q.waitForTimeout(5000);
    const stat = await q.textContent("#projstat");
    ok(before[0] !== before[1] && afterB === before[0] && rows.length === 1 && /2 (songs|canciones)/.test(rows[0]) && /(restored|restaurado)/i.test(stat),
      `projects: slots read (${before.join(", ")}), B saved; a backup kept (${rows[0]}), restored: "${stat}"`);
  }
  /* the manuals: JIANT FM's (the interactive manual opens here), JIANT BYTE's (what its site lacks says so) */
  {
    const m = await page("#docs", { width: 1280, height: 900 });
    await m.click('nav.views [data-go="docs"]'); await m.waitForTimeout(2500);
    const r = await m.evaluate(() => ({ cards: document.querySelectorAll("#doclist .doc").length,
      pdfs: [...document.querySelectorAll("#doclist a")].filter((a) => /cheatsheet-.*\.pdf$/.test(a.href)).length }));
    await m.click("#doclist .doc.wide .links button"); await m.waitForTimeout(1500);
    const f = await m.evaluate(() => ({ open: !document.getElementById("mantry").hidden, src: document.getElementById("f-manual").src }));
    await m.click('[data-fw="byte"]'); await m.click('nav.views [data-go="docs"]'); await m.waitForTimeout(2500);
    const by = await m.evaluate(() => ({ title: document.getElementById("docstitle").textContent, cards: document.querySelectorAll("#doclist .doc").length,
      // (its own cheat sheet linked once its site has it; until then "on its way", never JIANT FM's)
      pdf: [...document.querySelectorAll("#doclist a")].some((a) => a.href.includes("/JIANT-BYTE/docs/cheatsheet-byte-controls.pdf")) ||
           (document.querySelectorAll("#doclist .soon").length > 0 && ![...document.querySelectorAll("#doclist a")].some((a) => /\/JIANT-BYTE\/docs\/cheatsheet-controls/.test(a.href))) }));
    ok(r.cards === 7 && r.pdfs === 2 && f.open && /\/JIANT\/manual\/$/.test(f.src) && by.title === "JIANT BYTE" && by.cards === 4 && by.pdf,
      `manuals: JIANT FM ${r.cards} (2 cheat sheets, the interactive manual opens here), JIANT BYTE ${by.cards}, its own`);
  }
  /* install safety */
  const other = await p.context().newPage();               // (the same browser profile: BroadcastChannel reaches it)
  await other.goto(URL0); await other.waitForTimeout(800);
  await other.evaluate(() => { window.got = []; const bc = new BroadcastChannel("jiant-midi"); bc.onmessage = (e) => got.push(e.data); window.say = (m) => bc.postMessage(m); });
  await other.evaluate(() => say({ type: "who" })); await wait(300);
  const who = await other.evaluate(() => got.splice(0));
  await other.evaluate(() => say({ type: "installer", busy: true })); await wait(500);
  const rel = await other.evaluate(() => got.splice(0));
  const st = await p.evaluate(() => ({ dev: !!jiantStudio.state.dev, scr: jiantStudio.scr.on, busy: jiantStudio.state.busy }));
  await p.evaluate(() => jiantStudio.find()); await wait(300);
  const still = await p.evaluate(() => !!jiantStudio.state.dev);
  ok(who.some((m) => m.type === "studio" && m.connected) && rel.some((m) => m.type === "released" && m.who === "studio") && !st.dev && !st.scr && st.busy && !still,
    "an installer writes: Studio says released, lets go, screen off, no new link");
  await other.evaluate(() => say({ type: "installer", busy: false })); await wait(300);
  await p.evaluate(() => jiantStudio.find()); await wait(800);
  ok(await p.evaluate(() => !!jiantStudio.state.dev && !jiantStudio.state.busy), "the installer has finished: the FM-1 found again");
  /* target Emulator: the editor edits the emulator, the recorder records it */
  const wasm = join(ROOT, "build", "emu", "felucca.wasm");
  if (existsSync(wasm) && readFileSync(wasm).includes(Buffer.from("web_sysex"))) {
    const e = await page("?target=emu#emu", { width: 1280, height: 900 });
    const emu = e.frames().find((f) => f.url().includes("/JIANT/emu/"));
    await emu.click("#go"); await e.waitForTimeout(5000);
    await e.click('nav.views [data-go="editor"]'); await e.waitForTimeout(9000);
    const ed = e.frames().find((f) => f.url().includes("webapp/editor"));
    const conn = await ed.evaluate(() => document.getElementById("status").textContent);
    const before = await emu.evaluate(() => document.getElementById("screen").toDataURL());
    await ed.evaluate(() => { const s = document.querySelector("select[size]"); s.selectedIndex = 3; s.dispatchEvent(new Event("change", { bubbles: true })); });
    await e.waitForTimeout(2500);
    const after = await emu.evaluate(() => document.getElementById("screen").toDataURL());
    ok(/Connected|接続/.test(conn) && before !== after, "target Emulator: the editor connects to the emulator; a preset picked there shows on it");
    await e.click('nav.views [data-go="rec"]'); await e.click("#recgo"); await e.waitForTimeout(2000); await e.click("#recgo"); await e.waitForTimeout(800);
    const takes = await e.evaluate(() => jiantStudio.rec.takes.map((t) => ({ s: t.secs, f: t.files.map((x) => x.blob.size) })));
    ok(takes.length === 1 && takes[0].s > 1.5 && takes[0].f[0] > 44 + 44100 * 4, `the emulator's output recorded: ${takes[0] ? takes[0].s.toFixed(2) : 0} s, a stereo WAV`);
    /* the emulator as an input: its stems (8 channels, as USB STEMS) and its MIDI OUT, keys played on it */
    if (readFileSync(wasm).includes(Buffer.from("web_stems"))) {
      await e.selectOption("#recdev", "emu-stems"); await e.selectOption("#midisrc", "emu-midi");
      await e.click("#recgo"); await e.waitForTimeout(500);
      await e.evaluate(() => document.getElementById("f-emu").contentWindow.focus());
      for (const k of ["KeyZ", "KeyC", "KeyB"]) { await e.keyboard.down(k); await e.waitForTimeout(300); await e.keyboard.up(k); await e.waitForTimeout(150); }
      await e.waitForTimeout(800); await e.click("#recgo"); await e.waitForTimeout(1000);
      const st = await e.evaluate(() => { const tk = jiantStudio.rec.takes[0];
        return { ch: tk.planes.length, peak: tk.planes.map((p) => { let m = 0; for (const v of p) m = Math.max(m, Math.abs(v)); return m; }),
          files: tk.files.map((f) => f.label), notes: jiantStudio.rec.midi.filter((m) => (m.data[0] & 0xF0) === 0x90 && m.data[2]).length }; });
      ok(st.ch === 8 && st.peak.some((v) => v > 0.05) && st.files.length >= 9 && st.notes === 3,
        `the emulator as an input: ${st.ch} stems (${st.peak.filter((v) => v > 0.05).length} with sound), ${st.files.length} files, ${st.notes} notes from its MIDI OUT`);
    }
  } else console.log("studio browser: (no emulator with the editor in build/emu: web/emu/build.sh; the target Emulator checks skipped)");
  ok(!errs.length, `no page errors${errs.length ? ": " + errs.slice(0, 3).join(" | ") : ""}`);
} finally {
  await b.close();
  server.kill();
}
console.log(failed ? `STUDIO BROWSER TESTS FAILED (${failed})` : "studio browser tests passed");
process.exit(failed ? 1 : 0);
