// SPDX-License-Identifier: GPL-3.0-only
// The beta emulator's version picker (the owner: "the beta emulator should play every version the public installer
// offers, to compare parameters and adjust or go back"). Loaded by preview/emu/index.html (injected by emulator.yml
// and split-beta.yml) and by every emu-v/<sha7>/index.html (emulator.yml). emu-v/index.json lists each version the
// public installer offers (web/STABLE, then web/CLASSIC), each built from its own commit with its own page, worklet
// and flash key (felucca-emu-flash-<sha7>). On the beta page another version opens in a frame over it and the beta's
// audio is suspended; "Beta" closes the frame and resumes it. A classic script before the page's module: it wraps
// AudioContext to know the page's contexts.
(() => {
  "use strict";
  const KEY = "jiant-emu-version", WARNED = "jiant-emu-version-warned";
  const get = (k) => { try { return localStorage.getItem(k); } catch (_) { return null; } };
  const put = (k, v) => { try { v === null ? localStorage.removeItem(k) : localStorage.setItem(k, v); } catch (_) {} };
  const script = document.currentScript;
  const base = new URL(".", script ? script.src : location.href);   // .../emu-v/
  const inVersion = /\/emu-v\/[0-9a-f]{7}\/?/.test(location.pathname);
  if (inVersion && window.top !== window) return;                    // a version inside the beta page's frame
  const ctxs = [];
  if (!inVersion && window.AudioContext) {
    const Orig = window.AudioContext;
    window.AudioContext = class extends Orig { constructor(...a) { super(...a); ctxs.push(this); } };
    window.jiantEmuContexts = ctxs;
  }
  const css = `#jv-bar{position:fixed;top:8px;right:8px;z-index:2147483647;display:flex;gap:6px;align-items:center;
    background:#16131b;color:#ece6dc;border:1px solid #3a3440;border-radius:10px;padding:6px 8px;
    font:13px/1.2 system-ui,sans-serif;box-shadow:0 4px 16px #0008;max-width:calc(100vw - 16px)}
    #jv-bar select{background:#221d29;color:#ece6dc;border:1px solid #3a3440;border-radius:6px;padding:3px 4px;font:inherit;max-width:58vw}
    #jv-bar b{color:#f2c230;font-weight:600}
    #jv-frame{position:fixed;inset:0;width:100vw;height:100vh;border:0;z-index:2147483646;background:#0d0b10}
    #jv-note{position:fixed;top:52px;right:8px;z-index:2147483647;max-width:min(360px,calc(100vw - 16px));background:#2a2210;
    color:#ece6dc;border:1px solid #f2c230;border-radius:10px;padding:8px 10px;font:12px/1.4 system-ui,sans-serif}
    #jv-note button{margin-top:6px;background:#f2c230;color:#16131b;border:0;border-radius:6px;padding:3px 10px;font:inherit}`;
  const ready = (f) => document.readyState === "loading" ? document.addEventListener("DOMContentLoaded", f) : f();
  ready(async () => {
    const st = document.createElement("style"); st.textContent = css; document.head.appendChild(st);
    const bar = document.createElement("div"); bar.id = "jv-bar";
    const label = document.createElement("span"); label.textContent = "Version";
    const sel = document.createElement("select"); sel.id = "jv-select"; sel.setAttribute("aria-label", "Firmware version");
    bar.append(label, sel); document.body.appendChild(bar);
    const pageVer = (document.querySelector(".version") || {}).textContent || "";
    let list = [];
    try { list = await (await fetch(new URL("index.json", base), { cache: "no-cache" })).json(); } catch (_) {}
    const here = inVersion ? location.pathname.match(/\/emu-v\/([0-9a-f]{7})/)[1] : "beta";
    const add = (value, text, ok = true) => { const o = document.createElement("option"); o.value = value; o.textContent = text; o.disabled = !ok; sel.appendChild(o); };
    add("beta", "Beta " + pageVer.replace(/\s*\(preview\)\s*/, "").trim() + (inVersion ? "" : " (this page)"));
    for (const v of list) add(v.sha7, v.version + (v.public ? " (public)" : "") + (v.ok ? "" : " (no emulator)"), v.ok);
    sel.value = here;
    let frame = null;
    const note = () => {
      if (get(WARNED)) return;
      const n = document.createElement("div"); n.id = "jv-note";
      n.innerHTML = "Each version keeps its own memory (projects, sounds) in this browser. An older version may not read a project made on a newer one: save it from the newer version first.<br>";
      const ok = document.createElement("button"); ok.textContent = "OK";
      ok.onclick = () => { put(WARNED, "1"); n.remove(); }; n.appendChild(ok); document.body.appendChild(n);
    };
    const open = (sha) => {
      if (inVersion) { location.href = sha === "beta" ? new URL("../../preview/emu/", base).href : new URL(sha + "/", base).href; return; }
      if (sha === "beta") {
        if (frame) { frame.remove(); frame = null; }
        for (const c of ctxs) try { c.resume(); } catch (_) {}
        document.title = document.title.replace(/^\[[^\]]*\] /, "");
        return;
      }
      for (const c of ctxs) try { c.suspend(); } catch (_) {}
      if (!frame) { frame = document.createElement("iframe"); frame.id = "jv-frame"; frame.allow = "midi *; autoplay *; microphone *"; document.body.appendChild(frame); }
      frame.src = new URL(sha + "/", base).href;
      const v = list.find((x) => x.sha7 === sha);
      document.title = "[" + (v ? v.version : sha) + "] " + document.title.replace(/^\[[^\]]*\] /, "");
      note();
    };
    // (the owner: "aparece la 48 en el emulador") the version picked is reopened only while the beta is the one it was
    // picked over: a new beta opens as itself
    sel.addEventListener("change", () => { put(KEY, sel.value === "beta" ? null : sel.value + "@" + pageVer.trim()); open(sel.value); });
    if (!inVersion) {
      const saved = (get(KEY) || "").split("@");
      if (saved[0] && saved[1] === pageVer.trim() && list.some((v) => v.sha7 === saved[0] && v.ok)) { sel.value = saved[0]; open(saved[0]); }
      else if (saved[0]) put(KEY, null);
    }
  });
})();
