// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// (V10.7.36 emu-transfer) The emulator page's FM-1 panel: SEND TO FM-1 / GET FROM FM-1 (a project, a user preset or
// everything, over USB Web MIDI; Bluetooth MIDI marked EXPERIMENTAL), DOWNLOAD BACKUP / IMPORT BACKUP (the emulator's
// flash as the installer's and the editor's backup file). The work is transfer.js's; this file is the page's part.
// English and Spanish (the browser's language, a switch in the panel).
//   const x = mountTransfer(document.getElementById("xfer"));   // at load: the panel, its buttons waiting
//   x.attach(node);                                              // once the emulator is on (its AudioWorkletNode)
import * as X from "./transfer.js";

const T = {
  en: {
    title: "FM-1 ⇄ emulator: send what you made to your FM-1, get it back, backups",
    emu: "Emulator", fm1: "FM-1", off: "switch the emulator on first", notConn: "not connected", connect: "Connect FM-1",
    reconnect: "Reconnect", usb: "USB", bt: "Bluetooth", what: "What", project: "Project", preset: "User preset", all: "Everything",
    send: "SEND TO FM-1 →", get: "← GET FROM FM-1", download: "DOWNLOAD BACKUP", importB: "IMPORT BACKUP", retry: "RETRY",
    settings: "also the settings (MENU options, palette, button calibration)",
    emuSlot: "Emulator slot", fm1Slot: "FM-1 slot", empty: "(empty)", noName: "(no name)",
    allDesc: "every project (1–6) with its song rows, the 32 user presets, the music playing now",
    btWarn: "EXPERIMENTAL: the FM-1 is connected over Bluetooth MIDI. It is slow and may drop in the middle; USB is recommended. A dropped transfer says what to do and can be retried.",
    help: "Chrome or Edge on a computer, or Chrome on Android with a USB OTG cable. Transfers stop playback on both. On iPhone / iPad: DOWNLOAD BACKUP here and restore the file later with the editor on a computer.",
    connecting: "Looking for the FM-1…", reading: "Reading", writing: "Writing", verifying: "Verifying…",
    connected: (v, how) => `${v} over ${how}`, olderNote: (e) => `(the emulator is ${e})`,
    askTitle: (where) => `This replaces on the ${where}:`, askNone: (where) => `Nothing stored there yet on the ${where}.`,
    askSave: (where) => `Save a backup of the ${where} first`, askGo: "Replace", askGoNew: "Copy", cancel: "Cancel",
    saved: (n) => `Backup saved: ${n}. Keep it until you check the result.`,
    done: (what, where) => `Done: ${what} on the ${where}, written and verified.`,
    doneAll: (n, where) => `Done: ${n} objects on the ${where}, written and verified.`,
    downloaded: (n) => `Downloaded ${n}: restore it with the editor (Complete backup > Restore backup) on any FM-1 with JIANT V10.6p or later, or IMPORT BACKUP here.`,
    imported: (n) => `Imported: ${n} objects into the emulator, verified.`, songsLeft: (s) => ` Songs ${s} of that backup were not taken (this firmware keeps 6).`,
    projectN: (n) => `PROJECT ${n}`, presetN: (n) => `user preset ${n}`, everything: "everything",
    rows: "its song rows", bank1: "user presets 1–16", bank2: "user presets 17–32", patches: "the presets' FM6 patches", live: "the music playing", settingsW: "the settings",
    sections: (s, k) => `PROJECT ${s + 1} sections ${k.split("").join(", ")}`,
    err: {
      nomidi: "This browser has no Web MIDI. Use Chrome or Edge on a computer, or Chrome on Android (USB OTG). On iPhone / iPad: DOWNLOAD BACKUP, then restore it with the editor on a computer.",
      denied: "MIDI access was not allowed. Allow MIDI (with SysEx) for this site in the browser's settings and try again.",
      notfound: "No FM-1 answered. Plug it in over USB, switch it on (JIANT running, not the update screen), close the editor or other pages using it, and press Connect FM-1.",
      fwOld: (v) => `The FM-1's firmware (${v || "unknown"}) is too old for this: it needs JIANT V10.6p or later (6 songs, backup restore). Update it with the installer first (it offers a backup before installing).`,
      fwOldFormat: (v) => `The FM-1's firmware (${v}) stores projects in an older format than this emulator's and would refuse them. Update the FM-1 with the installer to the emulator's version first. User presets can still be sent.`,
      emuOld: (v) => `The FM-1 (${v}) is newer than this emulator: reload the page to get the current emulator, then try again.`,
      playing: "The device did not stop playing. Stop it and try again.",
      emptySlot: (n) => `User preset ${n} is empty: pick a stored one.`,
      lost: "The connection dropped before anything was written: nothing changed. Reconnect the FM-1 (USB recommended) and press RETRY.",
      verify: (ids) => `Written, but the read-back does not match (${ids}). Press RETRY; if it happens again, restore the backup you saved.`,
      stale: "The FM-1 reset the transfer (another page or the editor used it, or it was idle too long). Press RETRY.",
      changed: "The data changed while it was being read. Stop playback and press RETRY.",
      flash: "The FM-1 could not write its flash. Press RETRY; if it happens again, power-cycle it.",
      validation: "The device refused the data (it failed its checks). Nothing of that object was written.",
      busy: "A transfer is running.",
      file: (m) => `Not a JIANT / Felucca backup file: ${m}`,
      partial: (where, newP, oldP, lost) => `${lost ? "The connection dropped" : "The transfer stopped"} half-way. On the ${where} these are NEW: ${newP}; these are still OLD: ${oldP}. The project plays a mix of both until it is finished. Press RETRY to send it all again (reconnect first), or restore the backup you saved.`,
      other: (m) => `Error: ${m}`,
    },
  },
  es: {
    title: "FM-1 ⇄ emulador: pasá lo que hiciste a tu FM-1, traelo de vuelta, backups",
    emu: "Emulador", fm1: "FM-1", off: "primero encendé el emulador", notConn: "no conectado", connect: "Conectar FM-1",
    reconnect: "Reconectar", usb: "USB", bt: "Bluetooth", what: "Qué", project: "Proyecto", preset: "Preset de usuario", all: "Todo",
    send: "ENVIAR AL FM-1 →", get: "← TRAER DEL FM-1", download: "DESCARGAR BACKUP", importB: "IMPORTAR BACKUP", retry: "REINTENTAR",
    settings: "también los ajustes (opciones del MENU, paleta, calibración de botones)",
    emuSlot: "Lugar en el emulador", fm1Slot: "Lugar en el FM-1", empty: "(vacío)", noName: "(sin nombre)",
    allDesc: "todos los proyectos (1–6) con sus filas de canción, los 32 presets de usuario, la música que suena ahora",
    btWarn: "EXPERIMENTAL: el FM-1 está conectado por MIDI Bluetooth. Es lento y se puede cortar en el medio; se recomienda USB. Si se corta, te dice qué hacer y podés reintentar.",
    help: "Chrome o Edge en una computadora, o Chrome en Android con un cable USB OTG. Las transferencias detienen la reproducción en ambos. En iPhone / iPad: DESCARGAR BACKUP acá y restaurar el archivo después con el editor en una computadora.",
    connecting: "Buscando el FM-1…", reading: "Leyendo", writing: "Escribiendo", verifying: "Verificando…",
    connected: (v, how) => `${v} por ${how}`, olderNote: (e) => `(el emulador es ${e})`,
    askTitle: (where) => `Esto reemplaza en el ${where}:`, askNone: (where) => `Todavía no hay nada guardado ahí en el ${where}.`,
    askSave: (where) => `Guardar antes un backup del ${where}`, askGo: "Reemplazar", askGoNew: "Copiar", cancel: "Cancelar",
    saved: (n) => `Backup guardado: ${n}. Guardalo hasta comprobar el resultado.`,
    done: (what, where) => `Listo: ${what} en el ${where}, escrito y verificado.`,
    doneAll: (n, where) => `Listo: ${n} objetos en el ${where}, escritos y verificados.`,
    downloaded: (n) => `Descargado ${n}: restauralo con el editor (Complete backup > Restore backup) en cualquier FM-1 con JIANT V10.6p o posterior, o con IMPORTAR BACKUP acá.`,
    imported: (n) => `Importado: ${n} objetos en el emulador, verificados.`, songsLeft: (s) => ` Las canciones ${s} de ese backup no se tomaron (este firmware guarda 6).`,
    projectN: (n) => `el PROYECTO ${n}`, presetN: (n) => `el preset de usuario ${n}`, everything: "todo",
    rows: "sus filas de canción", bank1: "presets de usuario 1–16", bank2: "presets de usuario 17–32", patches: "los patches FM6 de los presets", live: "la música que suena", settingsW: "los ajustes",
    sections: (s, k) => `PROYECTO ${s + 1} secciones ${k.split("").join(", ")}`,
    err: {
      nomidi: "Este navegador no tiene Web MIDI. Usá Chrome o Edge en una computadora, o Chrome en Android (USB OTG). En iPhone / iPad: DESCARGAR BACKUP y restauralo con el editor en una computadora.",
      denied: "No se permitió el acceso MIDI. Permití MIDI (con SysEx) para este sitio en los ajustes del navegador y probá de nuevo.",
      notfound: "Ningún FM-1 respondió. Conectalo por USB, encendelo (con JIANT, no en la pantalla de actualización), cerrá el editor u otras páginas que lo usen y tocá Conectar FM-1.",
      fwOld: (v) => `El firmware del FM-1 (${v || "desconocido"}) es demasiado viejo para esto: necesita JIANT V10.6p o posterior (6 canciones, restauración de backup). Actualizalo primero con el instalador (ofrece un backup antes de instalar).`,
      fwOldFormat: (v) => `El firmware del FM-1 (${v}) guarda los proyectos en un formato más viejo que el de este emulador y los rechazaría. Actualizá el FM-1 con el instalador a la versión del emulador. Los presets de usuario sí se pueden enviar.`,
      emuOld: (v) => `El FM-1 (${v}) es más nuevo que este emulador: recargá la página para tener el emulador actual y probá de nuevo.`,
      playing: "El equipo no dejó de reproducir. Detenelo y probá de nuevo.",
      emptySlot: (n) => `El preset de usuario ${n} está vacío: elegí uno guardado.`,
      lost: "La conexión se cortó antes de escribir nada: no cambió nada. Reconectá el FM-1 (mejor por USB) y tocá REINTENTAR.",
      verify: (ids) => `Se escribió, pero la lectura de control no coincide (${ids}). Tocá REINTENTAR; si vuelve a pasar, restaurá el backup que guardaste.`,
      stale: "El FM-1 reinició la transferencia (otra página o el editor lo usó, o pasó mucho tiempo). Tocá REINTENTAR.",
      changed: "Los datos cambiaron mientras se leían. Detené la reproducción y tocá REINTENTAR.",
      flash: "El FM-1 no pudo escribir su memoria. Tocá REINTENTAR; si vuelve a pasar, apagalo y encendelo.",
      validation: "El equipo rechazó los datos (no pasaron sus controles). No se escribió nada de ese objeto.",
      busy: "Hay una transferencia en curso.",
      file: (m) => `No es un archivo de backup de JIANT / Felucca: ${m}`,
      partial: (where, newP, oldP, lost) => `${lost ? "La conexión se cortó" : "La transferencia se detuvo"} a la mitad. En el ${where} esto quedó NUEVO: ${newP}; esto sigue VIEJO: ${oldP}. El proyecto suena como una mezcla de ambos hasta terminarlo. Tocá REINTENTAR para enviarlo todo de nuevo (reconectá primero), o restaurá el backup que guardaste.`,
      other: (m) => `Error: ${m}`,
    },
  },
};

const CSS = `
#xfer .row { display: flex; flex-wrap: wrap; gap: 6px 10px; align-items: center; margin: 6px 0; }
#xfer select { font: inherit; max-width: 100%; min-width: 0; }
#xfer label.slot { display: flex; flex-direction: column; gap: 2px; flex: 1 1 13em; min-width: 0; }
#xfer button { font: inherit; padding: 4px 12px; background: #fff; color: #000; border: 0; cursor: pointer; }
#xfer button.go { font-weight: 700; letter-spacing: .03em; }
#xfer button:disabled { opacity: .4; cursor: default; }
#xfer button.lang { background: transparent; color: var(--fg); border: 1px solid var(--edge); padding: 1px 8px; }
#xfer .dev { color: var(--fg); }
#xfer .dev b { font-weight: 600; }
#xfer .warn { border-left: 3px solid #ff9f0a; padding: 4px 10px; background: rgba(255, 159, 10, .12); color: var(--fg); margin: 6px 0; }
#xfer .ask { border: 1px solid var(--edge); background: rgba(255, 255, 255, .25); padding: 6px 10px; margin: 6px 0; color: var(--fg); }
#xfer .ask ul { margin: 2px 0 6px; padding-left: 18px; }
#xfer progress { width: 100%; height: 10px; accent-color: #1d1d1f; }
#xfer .msg { color: var(--fg); margin: 4px 0; white-space: pre-line; }
#xfer .msg.bad { color: #8a1111; font-weight: 600; }
#xfer .msg.good { color: #0d4a22; font-weight: 600; }
#xfer fieldset { border: 0; padding: 0; margin: 0; display: flex; flex-wrap: wrap; gap: 4px 12px; }
#xfer small { color: var(--dim); }
@media (max-width: 520px) { #xfer .acts button { flex: 1 1 45%; } }
`;

export function mountTransfer(root, opt = {}) {
  let lang = (() => { try { return localStorage.getItem("jiant-emu-lang"); } catch { return null; } })() ||
             ((opt.lang || navigator.language || "en").slice(0, 2) === "es" ? "es" : "en");
  if (!T[lang]) lang = "en";
  const t = () => T[lang];
  if (!document.getElementById("xfer-css")) {
    const st = document.createElement("style"); st.id = "xfer-css"; st.textContent = CSS; document.head.append(st);
  }
  root.id = "xfer";
  root.classList.add("stems");
  /* the page's keyboard plays the emulator: keys pressed in this panel (a select's arrows) stay here */
  for (const ev of ["keydown", "keyup"]) root.addEventListener(ev, (e) => e.stopPropagation());

  const S = { emu: null, fm1: null, info: null, link: null, access: null, emuCat: null, fm1Cat: null, emuList: null, fm1List: null,
              what: "project", busy: false, retry: null, msg: "", msgKind: "", ask: null, progress: null };

  const h = (tag, attrs = {}, ...kids) => {
    const e = document.createElement(tag);
    for (const [k, v] of Object.entries(attrs)) {
      if (k.startsWith("on")) e.addEventListener(k.slice(2), v);
      else if (v === true) e.setAttribute(k, "");
      else if (v !== false && v != null) e.setAttribute(k, v);
    }
    for (const k of kids.flat()) if (k != null && k !== false) e.append(k);
    return e;
  };
  const fileIn = h("input", { type: "file", accept: ".json,application/json", hidden: true, onchange: () => importFile() });

  // ------------------------------------------------------------------ what each slot holds, as an option's text
  const songText = (s) => `${s.n + 1} · ${s.used ? (s.name || t().noName) + "  [" + s.stored.map((v, i) => v ? "ABCD"[i] : "-").join("") + "]" : t().empty}`;
  const presetText = (p, k) => `${String(k + 1).padStart(2, "0")} · ${p ? `${p.name} · ${p.engineName}` : t().empty}`;
  const options = (cat, sel) => {
    if (S.what === "all") return [h("option", { value: "all" }, t().allDesc)];
    if (!cat) return [h("option", { value: "" }, "—")];
    const list = S.what === "project" ? cat.songs.map((s) => [s.n, songText(s)]) : cat.presets.map((p, k) => [k, presetText(p, k)]);
    return list.map(([v, txt]) => h("option", { value: v, selected: String(v) === String(sel) }, txt));
  };
  let selEmu = 0, selFm1 = 0;

  function render() {
    const L = t(), on = !!S.emu, conn = !!S.fm1, idle = !S.busy;
    const how = S.fm1?.bluetooth ? L.bt : L.usb;
    root.replaceChildren(...[
      h("summary", {}, L.title),
      h("div", { class: "row" },
        h("span", { class: "dev" }, h("b", {}, L.emu + ": "), on ? (S.emu.version || "?") : L.off),
        h("span", { class: "dev" }, h("b", {}, L.fm1 + ": "), conn ? L.connected(S.info.version, how) : L.notConn,
          conn && S.emu?.version && !S.info.version.endsWith(S.emu.version) ? " " + L.olderNote(S.emu.version) : ""),
        h("button", { type: "button", disabled: !on || !idle, onclick: () => run(connect) }, conn ? L.reconnect : L.connect),
        h("button", { type: "button", class: "lang", onclick: () => { lang = lang === "es" ? "en" : "es"; try { localStorage.setItem("jiant-emu-lang", lang); } catch {} render(); } }, lang === "es" ? "EN" : "ES")),
      S.fm1?.bluetooth ? h("div", { class: "warn", role: "note" }, L.btWarn) : null,
      h("div", { class: "row" },
        h("fieldset", {}, h("legend", { hidden: true }, L.what),
          ...["project", "preset", "all"].map((w) => h("label", {}, h("input", { type: "radio", name: "xfer-what", value: w, checked: S.what === w, disabled: !idle,
            onchange: () => { S.what = w; selEmu = selFm1 = 0; render(); } }), " " + L[w])))),
      h("div", { class: "row" },
        h("label", { class: "slot" }, h("small", {}, L.emuSlot),
          h("select", { disabled: !on || !idle || S.what === "all", onchange: (e) => { selEmu = +e.target.value; } }, options(S.emuCat, selEmu))),
        h("label", { class: "slot" }, h("small", {}, L.fm1Slot),
          h("select", { disabled: !conn || !idle || S.what === "all", onchange: (e) => { selFm1 = +e.target.value; } }, options(S.fm1Cat, selFm1)))),
      S.what === "all" ? h("div", { class: "row" }, h("label", {}, h("input", { type: "checkbox", id: "xfer-settings", checked: S.settings, disabled: !idle, onchange: (e) => { S.settings = e.target.checked; } }), " " + L.settings)) : null,
      h("div", { class: "row acts" },
        h("button", { type: "button", class: "go", disabled: !on || !conn || !idle, onclick: () => run(() => start("to")) }, L.send),
        h("button", { type: "button", class: "go", disabled: !on || !conn || !idle, onclick: () => run(() => start("from")) }, L.get)),
      h("div", { class: "row acts" },
        h("button", { type: "button", disabled: !on || !idle, onclick: () => run(downloadEmu) }, L.download),
        h("button", { type: "button", disabled: !on || !idle, onclick: () => fileIn.click() }, L.importB), fileIn),
      S.ask ? askBox() : null,
      S.progress ? h("progress", { max: 100, value: S.progress.v }) : null,
      S.progress?.text ? h("div", { class: "msg" }, S.progress.text) : null,
      S.msg ? h("div", { class: "msg " + S.msgKind, role: "status" }, S.msg) : null,
      S.retry && idle ? h("div", { class: "row" }, h("button", { type: "button", class: "go", onclick: () => run(S.retry) }, L.retry)) : null,
      h("p", {}, h("small", {}, L.help))].flat().filter((k) => k != null && k !== false));
    if (S.keepOpen !== false) root.open = root.open || !!S.msg || !!S.ask;
  }
  const say = (msg, kind = "") => { S.msg = msg; S.msgKind = kind; render(); };
  const settingsOn = () => !!S.settings;

  // ------------------------------------------------------------------ the question before writing
  function askBox() {
    const a = S.ask, L = t(), where = a.dst === "fm1" ? L.fm1 : L.emu.toLowerCase();
    const label = (o) => o.kind === "project" ? `${L.projectN(o.n + 1)} “${o.name || L.noName}”` : `${L.presetN(o.n + 1)} “${o.name}”`;
    return h("div", { class: "ask", role: "alertdialog" },
      a.items.length ? [h("div", {}, L.askTitle(where)), h("ul", {}, a.items.slice(0, 8).map((o) => h("li", {}, label(o))), a.items.length > 8 ? h("li", {}, `+${a.items.length - 8}`) : null)]
                     : h("div", {}, L.askNone(where)),
      a.dst === "fm1" && S.fm1?.bluetooth ? h("div", { class: "warn" }, L.btWarn) : null,
      h("div", { class: "row" },
        h("button", { type: "button", onclick: () => run(() => saveBackupOf(a.dst)) }, L.askSave(where)),
        h("button", { type: "button", class: "go", onclick: () => { const go = a.go; S.ask = null; run(go); } }, a.items.length ? L.askGo : L.askGoNew),
        h("button", { type: "button", onclick: () => { S.ask = null; render(); } }, L.cancel)));
  }

  // ------------------------------------------------------------------ operations
  async function run(fn) {
    if (S.busy) { say(t().err.busy, "bad"); return; }
    S.busy = true; S.msg = ""; render();
    try { await fn(); }
    catch (e) { S.progress = null; say(errorText(e), "bad"); console.warn("JIANT transfer:", e); }
    finally { S.busy = false; render(); }
  }
  function errorText(e) {
    const E = t().err, where = S.lastDst === "emu" ? t().emu.toLowerCase() : t().fm1;
    if (e?.code === "partial") {
      const name = (p) => p.song != null ? t().sections(p.song, p.sections) : t()[{ rows: "rows", bank1: "bank1", bank2: "bank2", patches: "patches", live: "live", settings: "settingsW" }[p]] || p;
      return E.partial(where, X.describeIds(e.written).map(name).join("; "), X.describeIds(e.pending).map(name).join("; ") || "—", e.lost);
    }
    const f = E[e?.code];
    if (typeof f === "function") return f(e.detail || S.info?.version);
    if (f) return f;
    return E.other(e?.message || String(e));
  }
  const progress = (label) => (done, total, phase) => {
    S.progress = { v: Math.round(100 * done / Math.max(1, total)), text: phase === "verify" ? t().verifying : `${phase === "read" ? t().reading : t().writing}${label ? " " + label : ""}… ${Math.round(100 * done / Math.max(1, total))} %` };
    render();
  };

  async function connect() {
    if (!navigator.requestMIDIAccess) throw new X.XferError("nomidi");
    S.progress = { v: 0, text: t().connecting }; render();
    try { S.link?.close(); } catch {}
    S.fm1 = S.info = S.fm1Cat = null;
    try { S.access = await navigator.requestMIDIAccess({ sysex: true }); }
    catch (e) { throw new X.XferError("denied", e?.message); }
    const c = await X.connectFM1(S.access);
    S.fm1 = c.side; S.info = c.info; S.link = c.link;
    S.access.onstatechange = (ev) => {
      const p = ev.port;
      if (S.link && (p === S.link.input || p === S.link.output) && p.state === "disconnected") { S.link.close(); S.fm1 = null; render(); }
    };
    await refresh();
    S.progress = null;
  }
  /* both sides' contents for the pickers (each LIST stops that side's playback) */
  async function refresh() {
    if (S.emu) { S.emuList = await X.list(S.emu); S.emuCat = await X.catalog(S.emu, S.info?.engines || X.ENGINES, S.emuList); }
    if (S.fm1) {
      S.fm1List = await X.list(S.fm1);
      const e = S.emuList && X.compat(S.info, S.fm1List, S.emuList, "to", "preset");
      if (e) throw e;
      S.fm1Cat = await X.catalog(S.fm1, S.info?.engines || X.ENGINES, S.fm1List);
    }
    render();
  }
  async function ensureFm1() {
    if (S.fm1 && S.link && !S.link.closed) return;
    await connect();
  }

  function jobOf(dir) {
    const from = dir === "to" ? selEmu : selFm1, to = dir === "to" ? selFm1 : selEmu;
    if (S.what === "all") return { what: "all", settings: settingsOn() };
    return { what: S.what, from, to };
  }
  /* SEND (dir "to") or GET (dir "from"): the format checked, the question asked (with the backup offered), then the job */
  async function start(dir) {
    await ensureFm1();
    await refresh();
    const job = jobOf(dir), src = dir === "to" ? S.emu : S.fm1, dst = dir === "to" ? S.fm1 : S.emu;
    const e = X.compat(S.info, S.fm1List, S.emuList, dir, job.what);
    if (e) throw e;
    const srcCat = dir === "to" ? S.emuCat : S.fm1Cat, dstCat = dir === "to" ? S.fm1Cat : S.emuCat;
    if (job.what === "preset" && !srcCat.presets[job.from]) throw new X.XferError("emptySlot", String(job.from + 1));
    const dstName = dir === "to" ? "fm1" : "emu";
    S.ask = { dst: dstName, items: X.overwrites(dstCat, job), go: () => doJob(src, dst, job, dstName) };
    render();
  }
  async function doJob(src, dst, job, dstName) {
    S.lastDst = dstName;
    S.retry = null;
    const again = async () => { if (dstName === "fm1" || src.kind === "fm1") await connect(); await doJob(dstName === "fm1" ? S.emu : S.fm1, dstName === "fm1" ? S.fm1 : S.emu, job, dstName); };
    try {
      const L = t(), label = job.what === "project" ? L.projectN(job.to + 1) : job.what === "preset" ? L.presetN(job.to + 1) : L.everything;
      const r = await X.transfer(src, dst, job, progress(label));
      try { await refresh(); } catch {}                // (the pickers as they are now, then the message)
      S.progress = null;
      const where = dstName === "fm1" ? L.fm1 : L.emu.toLowerCase();
      say(job.what === "all" ? L.doneAll(r.verified.length, where) : L.done(label, where), "good");
    } catch (e) {
      if (["partial", "lost", "verify", "stale", "changed", "flash", "playing"].includes(e.code) || /timed out|closed/i.test(e.message)) S.retry = again;
      throw e;
    }
  }

  function download(name, obj) {
    const url = URL.createObjectURL(new Blob([JSON.stringify(obj)], { type: "application/json" }));
    const a = h("a", { href: url, download: name }); document.body.append(a); a.click(); a.remove();
    setTimeout(() => URL.revokeObjectURL(url), 2000);
  }
  async function saveBackupOf(who) {
    const side = who === "fm1" ? S.fm1 : S.emu;
    const file = await X.backupFile(side, progress(""));
    const name = X.backupName(who === "fm1" ? "fm1" : "emu");
    download(name, file);
    S.progress = null;
    say(t().saved(name), "good");
    if (who === "fm1") await refresh().catch(() => {});
  }
  async function downloadEmu() {
    const file = await X.backupFile(S.emu, progress(""));
    const name = X.backupName("emu");
    download(name, file);
    S.progress = null;
    say(t().downloaded(name), "good");
  }
  async function importFile() {
    const f = fileIn.files && fileIn.files[0];
    fileIn.value = "";
    if (!f || !S.emu) return;
    run(async () => {
      let text;
      try { if (f.size > 600000) throw new Error("too large"); text = await f.text(); X.readBackup(text); }
      catch (e) { throw new X.XferError("file", e.message); }
      await refresh();
      const job = { what: "all" };
      S.ask = { dst: "emu", items: X.overwrites(S.emuCat, job), go: async () => {
        S.lastDst = "emu";
        const r = await X.restoreFile(S.emu, text, { settings: settingsOn() }, progress(""));
        await refresh().catch(() => {});
        S.progress = null;
        say(t().imported(r.verified.length) + (r.songsLeft.length ? t().songsLeft(r.songsLeft.join(", ")) : ""), "good");
      } };
      render();
    });
  }

  render();
  return {
    /* the emulator is on: its worklet answers on a channel of its own */
    attach(node) {
      const ch = new MessageChannel();
      S.emu = X.emuSideFromPort(ch.port1);
      node.port.postMessage({ type: "edport", port: ch.port2 }, [ch.port2]);
      setTimeout(render, 200);
    },
    state: S, render,
  };
}
