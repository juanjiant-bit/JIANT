// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// (emu-transfer) The FM-1 panel in a real browser (Playwright's Chromium), after web/emu/build.sh:
//   node web/emu/browser_transfer_test.mjs [out-dir-for-screenshots]
// The page is build/emu as the site serves it; navigator.requestMIDIAccess is replaced by a simulated FM-1 (fm1mock.mjs)
// whose firmware is a second felucca.wasm (its own song made by the generator in PROJECT 3, a user preset 20). Checks:
// switch on, CONNECT FM-1 (its version, USB), the pickers show both sides, SEND a project asks first (the FM-1's
// PROJECT 3 named), REPLACE writes and verifies, GET a project back, DOWNLOAD BACKUP gives the editor's file, a pulled
// cable says what is half-written and RETRY finishes, a Bluetooth port shows the EXPERIMENTAL warning; phone width.
import fs from "fs";
import http from "http";
import path from "path";
import { createRequire } from "module";
const require = createRequire(import.meta.url);
const { chromium } = require("playwright");

const ROOT = path.resolve(path.dirname(new URL(import.meta.url).pathname), "../..");
const OUT = process.argv[2] || null;
const EMU = process.env.EMU_DIR || path.join(ROOT, "build/emu");     // (EMU_DIR: another built emu/, e.g. the beta shell's)
const SITE = { "/emu/fm1mock.mjs": "web/emu/fm1mock.mjs", "/fm1backup.js": "web/fm1backup.js" };
const TYPES = { ".html": "text/html", ".js": "text/javascript", ".mjs": "text/javascript", ".wasm": "application/wasm", ".woff": "font/woff", ".json": "application/json" };
const server = http.createServer((q, r) => {
  let u = decodeURIComponent(q.url.split("?")[0]);
  if (u === "/emu/") u = "/emu/index.html";
  const file = SITE[u] ? path.join(ROOT, SITE[u]) : u.startsWith("/emu/") ? path.join(EMU, u.slice(5)) : null;
  if (!file || !fs.existsSync(file)) { r.writeHead(404); r.end(); return; }
  r.writeHead(200, { "content-type": TYPES[path.extname(file)] || "application/octet-stream" }); r.end(fs.readFileSync(file));
});
await new Promise((res) => server.listen(0, res));
const base = `http://127.0.0.1:${server.address().port}/emu/`;

let fails = 0;
const check = (what, ok) => { console.log(`browser: ${what.padEnd(80)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };

/* the simulated FM-1 in the page: a second firmware, its PROJECT 3 a generated piece, a user preset in slot 20 */
const FAKE = ({ portName, pullAfter }) => {
  navigator.requestMIDIAccess = async () => {
    if (window.__fm1) return window.__fm1.access;
    const [{ jsDevice, wasmDevice, midiAccess }, { edServe, edVersion }] = await Promise.all([import("./fm1mock.mjs"), import("./worklet.js")]);
    const { instance } = await WebAssembly.instantiate(await (await fetch("felucca.wasm")).arrayBuffer(), {});
    const ex = instance.exports; ex._initialize(); ex.web_nor_erase(); ex.web_seed(5); ex.web_boot();
    for (let i = 0; i < 12; i++) ex.web_render(1024);
    ex.web_gen(3, 42, 70, 2); for (let i = 0; i < 30; i++) ex.web_render(1024);
    const X = await import("./transfer.js"), self = X.emuSideFromExports(ex, edServe);
    await X.transfer(self, self, { what: "project", from: 0, to: 2 });   // (its PROJECT 3: a piece of its own)
    const dev = wasmDevice(ex, edServe, "JIANT " + edVersion(ex));
    if (window.__debug) { const h = dev.handle; dev.handle = (c, a) => { const r = h(c, a); if (c === 67 && a[0] !== 1 || !r) console.warn("fm1", c, a.slice(0, 2), r && r.slice(0, 3)); return r; }; }
    const access = midiAccess(dev, { name: portName, pullAfter, delay: 1 });
    window.__fm1 = { ex, access, dev };
    return access;
  };
};

async function page(browser, opt = {}) {
  const ctx = await browser.newContext({ viewport: opt.phone ? { width: 390, height: 844 } : { width: 1100, height: 900 }, deviceScaleFactor: 2,
                                         locale: opt.lang || "en-US", acceptDownloads: true, isMobile: !!opt.phone, hasTouch: !!opt.phone });
  const p = await ctx.newPage();
  p.on("pageerror", (e) => console.log("pageerror:", e.message));
  p.on("console", (m) => { if (m.type() === "warning" || m.type() === "error") console.log("console:", m.text()); });
  if (process.env.DEBUG) await p.addInitScript(() => { window.__debug = 1; });
  await p.addInitScript(FAKE, { portName: opt.portName || "FM-1", pullAfter: opt.pullAfter ?? null });
  await p.goto(base);
  await p.click("#go");
  await p.waitForSelector("#start", { state: "hidden", timeout: 20000 });
  await p.waitForFunction(() => document.querySelector("#xfer .dev")?.textContent.includes("V10"), null, { timeout: 10000 });
  await p.evaluate(() => { document.getElementById("xfer").open = true; });
  return p;
}
const text = (p) => p.locator("#xfer").innerText();
const shot = async (p, name) => { if (OUT) { fs.mkdirSync(OUT, { recursive: true }); await p.locator("#xfer").screenshot({ path: path.join(OUT, name) }); } };
const waitMsg = async (p, re, ms = 60000) => {
  try { await p.waitForFunction((r) => new RegExp(r).test(document.querySelector("#xfer").innerText), re.source, { timeout: ms }); }
  catch (e) { console.log("panel:", await text(p)); throw e; }
};

const browser = await chromium.launch();
try {
  { /* USB: connect, send a project (asked first), get it back, download the backup */
    const p = await page(browser);
    await shot(p, "1-panel-before-connect.png");
    await p.getByRole("button", { name: "Connect FM-1" }).click();
    await waitMsg(p, /over USB/);
    const t = await text(p);
    check("connect: the FM-1 named with its firmware version, over USB", /FM-1: JIANT V10\.\d+\.\d+ over USB/.test(t));
    const opts = await p.locator("#xfer select").nth(1).locator("option").allInnerTexts();
    check(`the FM-1's projects listed with what is there (${opts[2]})`, opts.length === 6 && /^3 · .+\[ABCD\]|^3 · .+\[/.test(opts[2]));
    /* GET FROM FM-1 first: its PROJECT 3 into the emulator's empty PROJECT 1 (nothing to replace: said) */
    await p.locator("#xfer select").nth(0).selectOption("0");
    await p.locator("#xfer select").nth(1).selectOption("2");
    await shot(p, "2-connected.png");
    await p.getByRole("button", { name: "← GET FROM FM-1" }).click();
    await p.waitForSelector("#xfer .ask");
    check("GET into an empty emulator slot: said, nothing to replace", /Nothing stored there yet/.test(await p.locator("#xfer .ask").innerText()));
    await p.getByRole("button", { name: "Copy", exact: true }).click();
    await waitMsg(p, /Done: PROJECT 1 on the emulator, written and verified/);
    const emuOpts = await p.locator("#xfer select").nth(0).locator("option").allInnerTexts();
    check(`the emulator's PROJECT 1 has it now (${emuOpts[0]})`, /\[ABCD\]/.test(emuOpts[0]));
    /* SEND it to the FM-1's PROJECT 3: asked first, the FM-1's backup offered */
    await p.locator("#xfer select").nth(0).selectOption("0");
    await p.locator("#xfer select").nth(1).selectOption("2");
    await p.getByRole("button", { name: "SEND TO FM-1 →" }).click();
    await p.waitForSelector("#xfer .ask");
    const ask = await p.locator("#xfer .ask").innerText();
    check("SEND asks before replacing the FM-1's PROJECT 3, offers its backup first", /replaces on the FM-1/.test(ask) && /PROJECT 3/.test(ask) && /Save a backup of the FM-1 first/.test(ask));
    await shot(p, "3-ask-before-overwrite.png");
    const [dl] = await Promise.all([p.waitForEvent("download"), p.getByRole("button", { name: "Save a backup of the FM-1 first" }).click()]);
    const fm1file = JSON.parse(fs.readFileSync(await dl.path(), "utf8"));
    check(`the FM-1's backup saved first (${dl.suggestedFilename()}, ${fm1file.objects.length} objects)`, fm1file.format === "felucca-backup" && fm1file.objects.length === 35);
    await p.getByRole("button", { name: "Replace" }).click();
    await waitMsg(p, /Done: PROJECT 3 on the FM-1, written and verified/);
    await shot(p, "4-sent-verified.png");
    /* a user preset: the FM-1 has none: the emulator's empty slot refused */
    await p.getByLabel("User preset").click();
    const [dl2] = await Promise.all([p.waitForEvent("download"), p.getByRole("button", { name: "DOWNLOAD BACKUP" }).click()]);
    const f = JSON.parse(fs.readFileSync(await dl2.path(), "utf8"));
    check(`DOWNLOAD BACKUP: ${dl2.suggestedFilename()}, the editor's format (${f.objects.length} objects)`, /^JIANT-emu-backup-\d{4}-\d\d-\d\d\.json$/.test(dl2.suggestedFilename()) && f.format === "felucca-backup" && f.objects.length === 35);
    await shot(p, "5-download-backup.png");
    /* IMPORT BACKUP: the FM-1's file back into the emulator */
    await p.setInputFiles("#xfer input[type=file]", await dl.path());
    await p.waitForSelector("#xfer .ask");
    await p.getByRole("button", { name: "Replace" }).click();
    await waitMsg(p, /Imported: \d+ objects into the emulator, verified/);
    check("IMPORT BACKUP: the FM-1's file into the emulator, verified", true);
    await p.context().close();
  }
  { /* the cable pulled half-way: what is new, what old; RETRY after plugging in */
    const p = await page(browser, { pullAfter: 20 });
    await p.getByRole("button", { name: "Connect FM-1" }).click();
    await waitMsg(p, /over USB/);
    await p.locator("#xfer select").nth(1).selectOption("2");
    await p.getByRole("button", { name: "← GET FROM FM-1" }).click();          // (the emulator's PROJECT 1: a piece)
    await p.getByRole("button", { name: "Copy", exact: true }).click();
    await waitMsg(p, /Done: PROJECT 1 on the emulator/);
    await p.locator("#xfer select").nth(1).selectOption("1");                  // (to the FM-1's empty PROJECT 2)
    await p.getByRole("button", { name: "SEND TO FM-1 →" }).click();
    await p.getByRole("button", { name: "Copy", exact: true }).click();
    await waitMsg(p, /half-way/, 90000);
    const t = await text(p);
    check("dropped: the half-written project said (NEW sections, OLD rest, how to fix), RETRY offered", /NEW: PROJECT 2 sections A/.test(t) && /still OLD/.test(t) && /RETRY/.test(t));
    await shot(p, "6-dropped-half-way.png");
    await p.evaluate(() => window.__fm1.access.plug());
    await p.getByRole("button", { name: "RETRY" }).click();
    await waitMsg(p, /Done: PROJECT 2 on the FM-1, written and verified/, 90000);
    check("dropped: RETRY after plugging in again finishes it, verified", true);
    await p.context().close();
  }
  { /* Bluetooth, in Spanish, on a phone */
    const p = await page(browser, { portName: "FM-1 Bluetooth", lang: "es-AR", phone: true });
    await p.getByRole("button", { name: "Conectar FM-1" }).click();
    await waitMsg(p, /por Bluetooth/);
    const t = await text(p);
    check("Bluetooth: connected, marked EXPERIMENTAL with the warning (Spanish)", /EXPERIMENTAL/.test(t) && /se recomienda USB/.test(t));
    const wide = await p.evaluate(() => document.documentElement.scrollWidth <= innerWidth);
    check("phone width: no horizontal scroll", wide);
    await shot(p, "7-phone-bluetooth-es.png");
    await p.getByLabel("Todo").click();
    await p.getByRole("button", { name: "ENVIAR AL FM-1 →" }).click();
    await p.waitForSelector("#xfer .ask");
    await shot(p, "8-phone-ask-everything-es.png");
    await p.context().close();
  }
} finally {
  await browser.close();
  server.close();
}
console.log(fails ? `BROWSER TRANSFER TESTS FAILED (${fails})` : "browser: all ok");
process.exit(fails ? 1 : 0);
