// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// (ports) devices.html in a real browser (Playwright's Chromium), after web/emu/build.sh:
//   node web/emu/browser_mpc_test.mjs [out-dir-for-screenshots]
// The MPC Sample panel on the big CPUs' build (felucca_hp.wasm): it switches on as that build, a synthetic WAV added
// with "Add samples" goes into slot 1 through the SAMPLE BROWSER (SAMPLE, pad 1, LOAD), the browser is drawn over the
// screen, a SAMPLE track and a GRAIN track play it from a pad (the worklet's output is not silent; before the slot was
// loaded the same pad was), RESAMPLE records into slot 2 and comes back as slot02.wav, a slot downloads as a WAV, the
// slots and files are there again after a reload, MIC / LINE from a (fake) input; the FM-1 and SMK-37 panels still play.
import fs from "fs";
import http from "http";
import os from "os";
import path from "path";
import { createRequire } from "module";
const require = createRequire(import.meta.url);
const { chromium } = require("playwright");

const ROOT = path.resolve(path.dirname(new URL(import.meta.url).pathname), "../..");
const OUT = process.argv[2] || null;
const EMU = process.env.EMU_DIR || path.join(ROOT, "build/emu");
const TYPES = { ".html": "text/html", ".js": "text/javascript", ".mjs": "text/javascript", ".wasm": "application/wasm", ".woff": "font/woff", ".json": "application/json" };
const server = http.createServer((q, r) => {
  let u = decodeURIComponent(q.url.split("?")[0]);
  if (u === "/emu/") u = "/emu/index.html";
  const file = u.startsWith("/emu/") ? path.join(EMU, u.slice(5)) : u === "/fm1backup.js" ? path.join(ROOT, "web/fm1backup.js") : null;
  if (!file || !fs.existsSync(file) || fs.statSync(file).isDirectory()) { r.writeHead(404); r.end(); return; }
  r.writeHead(200, { "content-type": TYPES[path.extname(file)] || "application/octet-stream" }); r.end(fs.readFileSync(file));
});
await new Promise((res) => server.listen(0, res));
const base = `http://127.0.0.1:${server.address().port}/emu/`;

let fails = 0;
const check = (what, ok, extra = "") => { console.log(`devices: ${what.padEnd(78)} ${ok ? "ok" : "FAIL"} ${extra}`); if (!ok) fails++; };
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// a synthetic WAV: 0.8 s, 48 kHz, 16-bit stereo, a decaying 330 Hz tone with a click (resampled to 44.1 kHz on load)
function wav(seconds = 0.8, rate = 48000) {
  const n = Math.round(seconds * rate), b = Buffer.alloc(44 + n * 4);
  b.write("RIFF", 0); b.writeUInt32LE(36 + n * 4, 4); b.write("WAVEfmt ", 8); b.writeUInt32LE(16, 16); b.writeUInt16LE(1, 20);
  b.writeUInt16LE(2, 22); b.writeUInt32LE(rate, 24); b.writeUInt32LE(rate * 4, 28); b.writeUInt16LE(4, 32); b.writeUInt16LE(16, 34);
  b.write("data", 36); b.writeUInt32LE(n * 4, 40);
  for (let i = 0; i < n; i++) {
    const v = Math.sin(2 * Math.PI * 330 * i / rate) * Math.exp(-i / (rate * 0.35)) * 0.8;
    b.writeInt16LE(Math.round(v * 32767), 44 + i * 4); b.writeInt16LE(Math.round(v * 0.6 * 32767), 46 + i * 4);
  }
  return b;
}
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "jiant-mpc-"));
const WAV = path.join(tmp, "tone330.wav");
fs.writeFileSync(WAV, wav());

const browser = await chromium.launch({ args: ["--autoplay-policy=no-user-gesture-required", "--use-fake-ui-for-media-stream",
                                               "--use-fake-device-for-media-stream"] });
const ctx = await browser.newContext({ viewport: { width: 1180, height: 1000 }, deviceScaleFactor: 2, acceptDownloads: true,
                                       permissions: ["microphone"] });
const page = await ctx.newPage();
const errors = [];
page.on("pageerror", (e) => { errors.push(e.message); console.log("pageerror:", e.message); });
page.on("console", (m) => { if (m.type() === "error") console.log("console:", m.text()); });

const ctl = (name) => page.locator(`.device g.ctl[aria-label="${name.replace(/\n/g, "\\a ")}"]`);
const tap = async (name, ms = 120) => {                // a press held ms, as a finger would
  const box = await ctl(name).boundingBox();
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down(); await sleep(ms); await page.mouse.up(); await sleep(80);
};
const meter = () => page.evaluate(() => window.__devices.emu.ask({ type: "meter" }));
const state = () => page.evaluate(() => { const b = window.__devices.current()?.browser; return b && { open: b.open, slot: b.slot, file: b.file, source: b.source, slots: b.slots, rec: b.rec, status: b.status, files: window.__devices.lib.files.map((f) => f.name), map: window.__devices.lib.map }; });
const until = async (what, fn, ms = 8000) => { const t0 = Date.now(); let v; while (Date.now() - t0 < ms) { v = await fn(); if (v) return v; await sleep(100); } return v; };
const shot = async (name, sel = ".device") => { if (OUT) { fs.mkdirSync(OUT, { recursive: true }); await page.locator(sel).first().screenshot({ path: path.join(OUT, name) }); } };
// the screen's pixels (the 320 x 240 canvas): how many are JIANT's orange in a box
const orange = (x, y, w, h) => page.evaluate(([x, y, w, h]) => {
  const d = document.querySelector(".device canvas").getContext("2d").getImageData(x, y, w, h).data;
  let n = 0;
  for (let i = 0; i < d.length; i += 4) if (d[i] > 200 && d[i + 1] > 90 && d[i + 1] < 160 && d[i + 2] < 60) n++;
  return n;
}, [x, y, w, h]);

async function switchOn() {
  await page.locator(".device .start button").click();
  await page.waitForFunction(() => document.querySelector(".device .start")?.hidden, null, { timeout: 20000 });
  await sleep(2400);                                   // (the splash)
}

// ---------------------------------------------------------------- the MPC
await page.goto(base + "devices.html#mpc");
await page.waitForSelector(".device svg");
check("the MPC panel says it has the big-CPU engines", await page.locator(".note.big").filter({ hasText: "SAMPLE" }).count() === 1);
check("the samples panel: Add samples", await page.locator(".samples .add").count() === 1);
await switchOn();
check("the MPC runs felucca_hp.wasm (FELUCCA_HOST_POWER)", await page.evaluate(() => window.__devices.emu.hostPower));

await page.locator(".samples input[type=file]").setInputFiles(WAV);
let st = await until("file", async () => { const s = await state(); return s.files.includes("tone330.wav") && s; });
check("a WAV added with Add samples is in the files", !!st);

// silence first: SAMPLE on track 1, its slot empty, pad 8 (C4)
await page.evaluate(() => { const e = window.__devices.emu; e.send({ type: "test-engine", track: 0, engine: 4 }); });
await sleep(300);
await page.evaluate(() => { const e = window.__devices.emu; e.send({ type: "test-preset", track: 0, preset: 0 }); });
await sleep(300);
await meter();
await tap("Pad 8", 400); await sleep(200);
const quiet = await meter();
check("SAMPLE with an empty slot: a pad is silent", quiet.rms < 1e-4, `rms ${quiet.rms.toExponential(2)}`);

// the browser: SAMPLE, pad 1, LOAD
await tap("SAMPLE");
st = await state();
check("SAMPLE opens the SAMPLE BROWSER", st.open);
await sleep(200);
check("the browser is drawn over the screen (SAMPLES, the file picked, in orange)", (await orange(40, 0, 120, 20)) > 20 && (await orange(46, 46, 230, 16)) > 20);
await tap("Pad 1");
await tap("GLO");                                      // (LOAD: the button over the screen's right)
st = await until("slot 1", async () => { const s = await state(); return s.slots[0].frames > 0 && s; });
check("LOAD: slot 1 holds the WAV (resampled to 44.1 kHz, stereo, named)", !!st && Math.abs(st.slots[0].frames - 35280) < 50 && st.slots[0].ch === 2 && st.slots[0].name === "tone330",
      st ? `${st.slots[0].frames} frames ${st.slots[0].ch} ch "${st.slots[0].name}"` : "");
check("the slots' map names the file (kept for the next visit)", st && st.map[0] === "tone330.wav");
await sleep(300);
await shot("mpc-sample-browser.png");
await shot("mpc-sample-browser-page.png", "main");
await tap("SAMPLE");
check("SAMPLE again closes it", !(await state()).open);

// SAMPLE plays slot 1 from a pad
await meter();
await tap("Pad 8", 400); await sleep(300);
const loud = await meter();
check("a SAMPLE preset played from a pad sounds (the worklet's output)", loud.rms > 3e-3 && loud.peak > 0.02, `rms ${loud.rms.toFixed(4)} peak ${loud.peak.toFixed(3)}`);
await tap("EDIT"); await sleep(500);
await shot("mpc-sample-edit.png");
await tap("◀ HOME"); await sleep(300);

// GRAIN on the same slot
await page.evaluate(() => window.__devices.emu.send({ type: "test-engine", track: 0, engine: 8 }));
await sleep(300);
await page.evaluate(() => window.__devices.emu.send({ type: "test-preset", track: 0, preset: 0 }));
await sleep(300);
await meter();
await tap("Pad 8", 700); await sleep(200);
const grain = await meter();
check("a GRAIN preset played from a pad sounds", grain.rms > 1e-3, `rms ${grain.rms.toFixed(4)}`);
await tap("EDIT"); await sleep(600);
await shot("mpc-grain-edit.png");
await tap("◀ HOME"); await sleep(300);

// RESAMPLE into slot 2: SAMPLE, pad 2, SAMPLE REC, a note, SAMPLE REC
await page.evaluate(() => { const e = window.__devices.emu; e.send({ type: "test-engine", track: 0, engine: 4 }); });
await sleep(300);
await page.evaluate(() => window.__devices.emu.send({ type: "test-preset", track: 0, preset: 0 }));
await tap("SAMPLE"); await tap("Pad 2");
st = await state();
check("the source is RESAMPLE at first", st.source === 0);
await tap("SAMPLE\nREC");
st = await until("rec", async () => { const s = await state(); return s.rec.st === 2 && s; }, 3000);
check("SAMPLE REC records (RECORDING)", !!st);
await tap("SAMPLE"); await tap("Pad 8", 300); await sleep(700); await tap("SAMPLE");
await tap("SAMPLE\nREC");
st = await until("slot 2", async () => { const s = await state(); return s.rec.st === 0 && s.slots[1].frames > 0 && s; }, 3000);
check("SAMPLE REC again stops: slot 2 holds the recording", !!st, st ? `${(st.slots[1].frames / 44100).toFixed(2)} s` : "");
st = await until("written back", async () => { const s = await state(); return s.files.includes("slot02.wav") && s.map[1] === "slot02.wav" && !s.slots[1].changed && s; }, 6000);
check("the recording comes back as slot02.wav in the files, the map names it", !!st);
const dl = page.waitForEvent("download", { timeout: 5000 }).catch(() => null);
await page.locator(".samples .slots li").nth(1).locator("button").click();
const got = await dl;
let ok = false;
if (got) { const p = await got.path(); const b = fs.readFileSync(p); ok = b.toString("ascii", 0, 4) === "RIFF" && b.readUInt32LE(24) === 44100 && b.length > 44 + 1000; }
check("a slot downloads as a WAV (16-bit, 44.1 kHz)", ok, got ? got.suggestedFilename() : "");

// ERASE clears the slot picked
await tap("ERASE");
st = await until("cleared", async () => { const s = await state(); return !s.slots[1].frames && s; }, 3000);
check("ERASE clears the slot", !!st && st.map[1] === undefined);

// LINE / MIC: this computer's input (a fake one here), asked for on the click
await tap("SAMPLE\nSELECT");
st = await until("line", async () => { const s = await state(); return s.source === 1 && /LINE IN/.test(s.status) && s; }, 5000);
check("SAMPLE SELECT: LINE asks for the input (granted: it is in)", !!st, (await state()).status);
await tap("Pad 3"); await tap("SAMPLE\nREC"); await sleep(800); await tap("SAMPLE\nREC");
st = await until("slot 3", async () => { const s = await state(); return s.slots[2].frames > 0 && s; }, 3000);
check("LINE records the input into slot 3", !!st && st.slots[2].name === "REC IN", st ? `${st.slots[2].name}` : "");
check("no page errors", errors.length === 0, errors.join("; "));

// a new visit: the files and slot 1 again
await page.reload();
await page.waitForSelector(".device svg");
await switchOn();
st = await until("restored", async () => { const s = await state(); return s.slots[0].frames > 0 && s; }, 8000);
check("after a reload the files are kept and slot 1 is loaded again", !!st && st.files.includes("tone330.wav") && st.slots[0].name === "tone330");

// ---------------------------------------------------------------- the others still play
await page.locator('.picker button[data-dev="smk37"]').click();
await page.waitForSelector(".device svg");
await switchOn();
check("SMK-37: felucca.wasm (not the big-CPU build)", !(await page.evaluate(() => window.__devices.emu.hostPower)));
await meter();
await page.locator('.device g.ctl[aria-label="Pad 9"]').hover();
await page.mouse.down(); await sleep(400); await page.mouse.up(); await sleep(200);
const smk = await meter();
check("SMK-37: a pad plays", smk.rms > 1e-3, `rms ${smk.rms.toFixed(4)}`);
check("SMK-37: no samples panel", await page.locator(".samples").count() === 0);

await page.locator('.picker button[data-dev="fm1"]').click();
const frame = await (await page.waitForSelector("iframe.fm1")).contentFrame();
await frame.waitForSelector("#go");
await frame.click("#go");
await frame.waitForFunction(() => document.getElementById("start")?.hidden || !document.getElementById("status")?.hidden, null, { timeout: 20000 });
const fm1 = await frame.evaluate(() => ({ on: document.getElementById("start").hidden, status: document.getElementById("status").textContent }));
if (!fm1.on && /ADDITIVE/.test(fm1.status)) console.log("devices: FM-1: skipped (the ADDITIVE shell with another edition's felucca.wasm: the site's pin is not here)");
else check("FM-1: the emulator (index.html) switches on in the page", fm1.on, fm1.status);
check("no page errors", errors.length === 0, errors.join("; "));

await browser.close();
server.close();
fs.rmSync(tmp, { recursive: true, force: true });
console.log(fails ? `devices: ${fails} FAILED` : "devices: all ok");
process.exit(fails ? 1 : 0);
