// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// JIANT Studio in Chromium (node web/studio/browser_test.mjs; Playwright): dev_server.py on a free port, then
// - the simulated FM-1 (?mock=1) found and named, the look by its firmware (MATH: JIANT BYTE);
// - the screen: SCREEN on, frames decoded and painted, full screen square in portrait and landscape;
// - install safety: an installer's "busy" on jiant-midi (from another tab): Studio answers "released", lets the FM-1 go,
//   stops the screen and refuses to find it again until the installer says it has finished;
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
  } else console.log("studio browser: (no emulator with the editor in build/emu: web/emu/build.sh; the target Emulator checks skipped)");
  ok(!errs.length, `no page errors${errs.length ? ": " + errs.slice(0, 3).join(" | ") : ""}`);
} finally {
  await b.close();
  server.kill();
}
console.log(failed ? `STUDIO BROWSER TESTS FAILED (${failed})` : "studio browser tests passed");
process.exit(failed ? 1 : 0);
