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
  /* wavetables (BYTE): a WAV of a gliding saw into 8 frames, sent to USER 3 of the simulated device, read back */
  {
    const q = await page("?mock=1&fw=byte#wt", { width: 1280, height: 900 });
    const sr = 44100, n = sr, d = Buffer.alloc(44 + n * 2); let ph = 0;
    d.write("RIFF", 0); d.writeUInt32LE(36 + n * 2, 4); d.write("WAVEfmt ", 8); d.writeUInt32LE(16, 16); d.writeUInt16LE(1, 20); d.writeUInt16LE(1, 22);
    d.writeUInt32LE(sr, 24); d.writeUInt32LE(sr * 2, 28); d.writeUInt16LE(2, 32); d.writeUInt16LE(16, 34); d.write("data", 36); d.writeUInt32LE(n * 2, 40);
    for (let i = 0; i < n; i++) { ph += 110 * (1 + i / n) / sr; d.writeInt16LE(Math.round(20000 * (2 * (ph - Math.floor(ph)) - 1)), 44 + i * 2); }
    await q.setInputFiles("#wtfile", { name: "glide.wav", mimeType: "audio/wav", buffer: d }); await q.waitForTimeout(1500);
    const frames = await q.$$eval("#wtframes canvas", (x) => x.length), name = await q.inputValue("#wtname");
    await q.click("#wtread"); await q.waitForTimeout(600);
    await q.selectOption("#wtslot", "2"); await q.click("#wtsend"); await q.waitForTimeout(2500);
    const sent = await q.textContent("#wtstat"), opt = await q.$eval('#wtslot option[value="2"]', (o) => o.textContent);
    const px = () => q.evaluate(() => window.__wtFrames.map((f) => Array.from(f, (v) => Math.round(v * 1000))));
    const before = await px();
    await q.click("#wtload"); await q.waitForTimeout(2500);
    const after = await px(), diff = before.flat().filter((v, i) => Math.abs(v - after.flat()[i]) > 1).length;
    const fmHidden = await (await page("?mock=1&fw=fm")).$eval('nav.views [data-go="wt"]', (e) => e.hidden);
    ok(frames === 8 && name === "GLIDE" && /GLIDE/.test(sent) && /GLIDE/.test(opt) && diff === 0 && fmHidden,
      `wavetables: glide.wav -> 8 frames, sent to USER 3 ("${sent}"), read back the same; hidden on FM`);
  }
  /* radio: the generator's pieces voted (the pretend generator of ?mock=1); a vote posted to Supabase (intercepted), a
     409 taken as done, a skip posts nothing */
  {
    const q = await page("?mock=1#radio", { width: 1280, height: 900 });
    const posts = [];
    await q.route("https://tjscbsiofkxshzckmmhk.supabase.co/**", (r) => { posts.push({ h: r.request().headers(), b: JSON.parse(r.request().postData()) }); r.fulfill({ status: posts.length === 1 ? 400 : posts.length === 3 ? 409 : 201, body: "" }); });
    await q.click("#rdgo"); await q.waitForTimeout(1500);
    const now = await q.textContent("#rdnow");
    await q.click("#rdup"); await q.click("#rdparts button:nth-child(1)"); await q.click("#rdparts button:nth-child(4)"); await q.click("#rdnext"); await q.waitForTimeout(800);
    await q.click("#rddown"); await q.waitForTimeout(5000);
    await q.click("#rdskip"); await q.waitForTimeout(800);
    const parts0 = posts[0] && posts[0].b.parts, noParts = posts[1] && !("parts" in posts[1].b);
    posts.shift();                                               // the first answer a 400 (no parts column): sent again without it
    const b0 = posts[0] && posts[0].b, count = await q.textContent("#rdcount");
    ok(now === "HOUSE" && posts.length === 2 && b0.device === "FM" && b0.vote === 1 && posts[1].b.vote === -1 && b0.genre === "HOUSE" && b0.version === "V10.8.44" &&
       /^c[0-9a-f]{24}$/.test(b0.client) && posts[0].h.apikey && posts[1].b.seed !== b0.seed && /: 2/.test(count) && parts0 === 9 && noParts && posts[1].b.parts === 0,
      `radio: ${now}, 👍 and 👎 posted (${posts.map((p) => p.b.vote).join(" ")}, a 409 taken), ⏭ nothing; parts 9 (a 400 sent again without it), none chosen 0 after 4 s; "${count}"`);
  }
  /* analysis: the simulated FM-1's six tracks, their parameters by name, steps, the song, in a summary */
  {
    const q = await page("?mock=1#an", { width: 1280, height: 900 });
    await q.click("#ango"); await q.waitForTimeout(12000);
    const txt = await q.textContent("#anpre");
    ok(/T1 ANALOG preset 1/.test(txt) && /CUT=90 \(def 64\)/.test(txt) && /steps with notes: 16/.test(txt) && /SONG 2 rows: Ax1 Bx2/.test(txt) && (await q.textContent("#anstat")) === "Done.",
      `analysis: 6 tracks, parameters by name, steps and the song in the summary (${txt.split("\n").length} lines)`);
  }
  /* rescue: the published JIANT FM and JieLi's loader fetched and checked, a board's fm1-rescue.uf2 made */
  {
    const ctx = await b.newContext({ viewport: { width: 1280, height: 900 }, acceptDownloads: true });
    const r = await ctx.newPage(); r.on("pageerror", (e) => errs.push(e.message));
    await r.goto(URL0 + "#rescue"); await r.waitForTimeout(800);
    await r.click('nav.views [data-go="rescue"]'); await r.waitForTimeout(8000);
    const fw = await r.textContent("#rsfwst"), ld = await r.textContent("#rsldst");
    await r.click('#rsboard [data-board="pico"]');
    const [dl] = await Promise.all([r.waitForEvent("download", { timeout: 20000 }).catch(() => null), r.click("#rsmake")]);
    const size = dl ? (await import("node:fs")).statSync(await dl.path()).size : 0;
    ok(/V10|JIANT/.test(fw) && /(checked|comprobado)/i.test(ld) && dl && dl.suggestedFilename() === "fm1-rescue.uf2" && size > 600000 && size % 512 === 0,
      `rescue: "${fw.trim()}", the loader checked, fm1-rescue.uf2 for the Pico made (${Math.round(size / 1024)} KB)`);
  }
  /* how it works: the index (every element a tile), a tile opens its page (its link), the animation runs; a link to
     a page opens it; JIANT BYTE's own */
  {
    const h = await page("#learn", { width: 1280, height: 900 });
    await h.click('nav.views [data-go="learn"]'); await h.waitForTimeout(1500);
    const tiles = await h.$$eval("#lrindex .lritile", (x) => x.length);
    await h.click("#lrindex .lrisec:nth-child(2) .lritile:nth-child(3)"); await h.waitForTimeout(800);
    const pg = await h.evaluate(() => ({ hash: location.hash, name: document.getElementById("lrname").textContent, how: document.querySelectorAll("#lrhow li").length }));
    const frame = () => h.evaluate(() => document.getElementById("lrcanvas").toDataURL());
    const f1 = await frame(); await h.waitForTimeout(400); const f2 = await frame();
    const d = await page("?fw=byte#learn/engines/wave", { width: 412, height: 900 });
    await d.waitForTimeout(1500);
    const dn = await d.evaluate(() => ({ name: document.getElementById("lrname").textContent, title: document.getElementById("lrtitle").textContent }));
    ok(tiles > 50 && /^#learn\/engines\//.test(pg.hash) && pg.name && pg.how >= 3 && f1 !== f2 && dn.name === "WAVE" && /BYTE/.test(dn.title),
      `how it works: ${tiles} elements in the index, "${pg.name}" at ${pg.hash} (moving), a link opens JIANT BYTE's ${dn.name}`);
  }
  /* how it works, live: the simulated FM-1 followed (WATCH); a knob turned (its CHANGED pushes) is named by DESC, its
     value shown, explained by the glossary and linked to the guide; a sound change told */
  {
    const v = await page("?mock=1#learn/live", { width: 1280, height: 900 });
    await v.waitForTimeout(1200);
    await v.click("#lvgo"); await v.waitForTimeout(2000);
    const a = await v.evaluate(() => ({ t: lvtitle.textContent, v: lvvaltext.textContent, what: lvwhat.textContent, more: !lvmore.hidden }));
    await v.waitForTimeout(3200);
    const log = await v.$$eval("#lvlog li", (x) => x.map((e) => e.textContent));
    ok(a.t === "CUT" && /^\d+$/.test(a.v) && a.what.length > 20 && !/parameter of this page|parámetro de esta/.test(a.what) && a.more && log.some((l) => /preset/.test(l)),
      `live: the FM-1 followed: ${a.t} = ${a.v}, "${a.what.slice(0, 50)}…", the guide linked, ${log.length} events (a sound change among them)`);
  }
  /* the quick tour: opens by itself the first time (no link to a view), walks every part (each lit), skipped: not again;
     the ? button opens it again */
  {
    const q = await (await b.newContext({ viewport: { width: 1280, height: 860 } })).newPage(); q.on("pageerror", (e) => errs.push(e.message));
    await q.goto(URL0); await q.waitForTimeout(1600);
    const first = await q.evaluate(() => !document.getElementById("tour").hidden);
    let lit = 0, n = 0;
    for (; n < 30; n++) {
      const st = await q.evaluate(() => ({ step: document.getElementById("tourstep").textContent, hole: getComputedStyle(document.getElementById("tourhole")).display !== "none", last: /Done|Listo/.test(document.getElementById("tournext").textContent) }));
      if (st.hole) lit++;
      if (st.last) break;
      await q.click("#tournext"); await q.waitForTimeout(120);
    }
    await q.click("#tournext"); await q.reload(); await q.waitForTimeout(1500);
    const again = await q.evaluate(() => !document.getElementById("tour").hidden);
    await q.click("#tourbtn"); await q.waitForTimeout(300);
    const reopened = await q.evaluate(() => !document.getElementById("tour").hidden);
    ok(first && n >= 15 && lit >= 15 && !again && reopened, `the quick tour: opens the first time, ${n + 1} steps (${lit} lit), not again once done, the ? opens it`);
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
