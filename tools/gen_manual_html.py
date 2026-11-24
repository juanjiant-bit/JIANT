#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""docs/MANUAL.md -> docs/guide/manual.html, docs/MANUAL.es.md -> docs/guide/manual.es.html.

One self-contained page per language (images inlined, works offline once downloaded): a table of contents from the
headings (beside the text on wide screens, folded on a phone), an "essential commands" card at the top, the EN/ES
switch and a Download link. Needs `pip install markdown`.

    python3 tools/gen_manual_html.py
"""
import base64
import html
import mimetypes
import os
import re
from pathlib import Path

import markdown

ROOT = Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"
OUT = DOCS / "guide"

# The commands everyone needs, taken from the manual and docs/cheatsheet/controls.html (V10.8.21).
ESSENTIAL = {
    "en": ("Essential commands", [
        ("Navigate", [
            ("<kbd>HOME</kbd> <kbd>EDIT</kbd> <kbd>SEQ</kbd> <kbd>FX</kbd> <kbd>GLO</kbd> <kbd>ENV</kbd> <kbd>LFO</kbd> <kbd>SCL</kbd> <kbd>ARP</kbd> <kbd>SAVE</kbd>",
             "From elsewhere: the page you were last on in that section. Already inside: the first page of the next group."),
            ("<kbd>SELECT</kbd>", "Walks every page of the section, a page a detent."),
            ("<kbd>ALGORITHM</kbd>", "Picks the track (T1–T4, or T1–T6 with TRACKS 6)."),
            ("<kbd>PRESETS</kbd>", "Always presets: the track's sounds. SEQ: the patterns. On SEQ › STEP FX it picks the step."),
            ("<kbd>KNOB 1–4</kbd>", "The four cards on screen. KNOB 1 picks (step, bar, slot, row)."),
            ("<kbd>HOME</kbd> held", "The MENU."),
        ]),
        ("Play and record", [
            ("<kbd>PLAY</kbd>", "Start / stop. Held about 0.7 s: PANIC, all notes off."),
            ("<kbd>GLO</kbd> + <kbd>PLAY</kbd>", "Restart from the top."),
            ("<kbd>REC</kbd>", "Arms live recording on the selected track. Held alone and let go: UNDO (again: redo)."),
            ("<kbd>OCT−</kbd> <kbd>OCT+</kbd>", "Keyboard octave; both together: back to 0. On action pages and dialogs: back / confirm."),
            ("<kbd>GLO</kbd> + <kbd>SELECT</kbd>", "Tempo, from any page."),
        ]),
        ("Hold a button (layers and combos)", [
            ("<kbd>FX</kbd> held", "Punch-in effects on the keys."),
            ("<kbd>LFO</kbd> held", "MACRO layer: KNOB 1–4 are M1–M4."),
            ("<kbd>SCL</kbd> held", "The keys set the root."),
            ("<kbd>HOME</kbd> + <kbd>SELECT</kbd>", "The GENERATOR."),
            ("<kbd>EDIT</kbd> + <kbd>SELECT</kbd>", "The sound dice (OCT+ keeps, OCT− cancels)."),
            ("<kbd>EDIT</kbd> + <kbd>PRESETS</kbd>", "The first sound of the next / previous engine."),
            ("<kbd>GLO</kbd> + <kbd>G4</kbd>", "UNDO the last big change."),
            ("<kbd>SEQ</kbd> + <kbd>G5</kbd>", "Clear the cursor step."),
            ("<kbd>SAVE</kbd> + <kbd>C5</kbd>", "NAME: rename."),
            ("<kbd>SEQ</kbd> + <kbd>REC</kbd>", "Clear all sequences (asks first)."),
        ]),
        ("New in V10.8.21", [
            ("HOME › JAM", "Keys F3–B3 pick A–D on every track, <kbd>OCT+</kbd> go, <kbd>OCT−</kbd> stop, <kbd>REC</kbd> records a take, K4 LOCK plays it."),
            ("SEQ › STEP FX", "Hold a white key: a step component plays; with <kbd>REC</kbd> it is written into the step."),
            ("SEQ › CLAY", "DRONE CLAY: a low drone and four modulators per track."),
        ]),
        ("Power-on", [
            ("<kbd>OCT−</kbd> + <kbd>OCT+</kbd> at power-on", "Hardware calibration."),
            ("<kbd>OCT−</kbd> + <kbd>OCT+</kbd> held 5 s", "Update mode (let go to cancel)."),
        ]),
    ]),
    "es": ("Comandos esenciales", [
        ("Navegar", [
            ("<kbd>HOME</kbd> <kbd>EDIT</kbd> <kbd>SEQ</kbd> <kbd>FX</kbd> <kbd>GLO</kbd> <kbd>ENV</kbd> <kbd>LFO</kbd> <kbd>SCL</kbd> <kbd>ARP</kbd> <kbd>SAVE</kbd>",
             "Desde otra sección: la última página que viste ahí. Ya adentro: la primera página del grupo siguiente."),
            ("<kbd>SELECT</kbd>", "Recorre todas las páginas de la sección, una por paso."),
            ("<kbd>ALGORITHM</kbd>", "Elige la pista (T1–T4, o T1–T6 con TRACKS 6)."),
            ("<kbd>PRESETS</kbd>", "Siempre presets: los sonidos de la pista. En SEQ: los patrones. En SEQ › STEP FX elige el paso."),
            ("<kbd>KNOB 1–4</kbd>", "Las cuatro tarjetas de la pantalla. KNOB 1 elige (paso, compás, slot, fila)."),
            ("<kbd>HOME</kbd> mantenido", "El MENU."),
        ]),
        ("Tocar y grabar", [
            ("<kbd>PLAY</kbd>", "Arranca / para. Mantenido unos 0,7 s: PANIC, corta todas las notas."),
            ("<kbd>GLO</kbd> + <kbd>PLAY</kbd>", "Vuelve al principio."),
            ("<kbd>REC</kbd>", "Arma la grabación en vivo de la pista elegida. Mantenido solo y soltado: UNDO (otra vez: rehace)."),
            ("<kbd>OCT−</kbd> <kbd>OCT+</kbd>", "Octava del teclado; las dos juntas: vuelve a 0. En páginas de acción y diálogos: atrás / confirmar."),
            ("<kbd>GLO</kbd> + <kbd>SELECT</kbd>", "El tempo, desde cualquier página."),
        ]),
        ("Mantener un botón (capas y combinaciones)", [
            ("<kbd>FX</kbd> mantenido", "Efectos punch-in en las teclas."),
            ("<kbd>LFO</kbd> mantenido", "Capa MACRO: KNOB 1–4 son M1–M4."),
            ("<kbd>SCL</kbd> mantenido", "Las teclas eligen la tónica."),
            ("<kbd>HOME</kbd> + <kbd>SELECT</kbd>", "El GENERATOR."),
            ("<kbd>EDIT</kbd> + <kbd>SELECT</kbd>", "El dado de sonido (OCT+ lo deja, OCT− lo cancela)."),
            ("<kbd>EDIT</kbd> + <kbd>PRESETS</kbd>", "El primer sonido del motor siguiente / anterior."),
            ("<kbd>GLO</kbd> + <kbd>G4</kbd>", "UNDO del último cambio grande."),
            ("<kbd>SEQ</kbd> + <kbd>G5</kbd>", "Borra el paso del cursor."),
            ("<kbd>SAVE</kbd> + <kbd>C5</kbd>", "NAME: cambiar el nombre."),
            ("<kbd>SEQ</kbd> + <kbd>REC</kbd>", "Borra todas las secuencias (pregunta antes)."),
        ]),
        ("Nuevo en V10.8.21", [
            ("HOME › JAM", "Las teclas F3–B3 eligen A–D en todas las pistas, <kbd>OCT+</kbd> lanza, <kbd>OCT−</kbd> para, <kbd>REC</kbd> graba una toma, K4 LOCK la reproduce."),
            ("SEQ › STEP FX", "Mantené una tecla blanca: suena un componente de paso; con <kbd>REC</kbd> queda grabado en el paso."),
            ("SEQ › CLAY", "DRONE CLAY: un drone grave y cuatro moduladores por pista."),
        ]),
        ("Al encender", [
            ("<kbd>OCT−</kbd> + <kbd>OCT+</kbd> al encender", "Calibración del hardware."),
            ("<kbd>OCT−</kbd> + <kbd>OCT+</kbd> mantenidos 5 s", "Modo de actualización (soltá para cancelar)."),
        ]),
    ]),
}

UI = {
    "en": {"title": "JIANT FM — User Manual", "toc": "Contents", "dl": "Download", "other": ("Español", "manual.es.html"),
           "new": ("What's new in V10.8.21", "whats-new-v10.8.21.html"), "self": "manual.html"},
    "es": {"title": "JIANT FM — Manual de uso", "toc": "Índice", "dl": "Descargar", "other": ("English", "manual.html"),
           "new": ("Lo nuevo en V10.8.21", "whats-new-v10.8.21.es.html"), "self": "manual.es.html"},
}

CSS = """
:root{--bg:#050409;--surf:#0b0912;--raise:#100e18;--line:rgba(239,230,200,.14);--fg:#efe6c8;--mid:#c9c1d8;--dim:#a39bb6;
--accent:#ff7a1a;--vio:#b870ff;--cyan:#3fe0e6;--heat:linear-gradient(90deg,#6b5cff,#b870ff 35%,#ff4bb0 60%,#ff7a1a 85%,#ffd34d);
--display:"Michroma","Segoe UI",system-ui,sans-serif;--body:"Instrument Sans",system-ui,sans-serif;--mono:"IBM Plex Mono",ui-monospace,Menlo,monospace;color-scheme:dark}
*{box-sizing:border-box}
html{scroll-behavior:smooth}
body{margin:0;background:var(--bg);color:var(--fg);font-family:var(--body);font-size:16px;line-height:1.65;
 background-image:radial-gradient(900px 420px at 85% -120px,rgba(184,112,255,.16),transparent 70%),radial-gradient(700px 360px at -10% 0,rgba(255,122,26,.08),transparent 70%);background-repeat:no-repeat}
.top{position:sticky;top:0;z-index:5;background:rgba(5,4,9,.86);backdrop-filter:blur(10px);-webkit-backdrop-filter:blur(10px);border-bottom:1px solid var(--line)}
.top .in{max-width:1240px;margin:0 auto;padding:12px max(16px,env(safe-area-inset-left,0px));display:flex;align-items:center;gap:14px 22px;flex-wrap:wrap}
.brand{font-family:var(--display);font-size:17px;letter-spacing:.04em;color:var(--fg);text-decoration:none;white-space:nowrap}
.brand b{font-weight:400;background:var(--heat);-webkit-background-clip:text;background-clip:text;color:transparent}
.brand small{font-family:var(--mono);font-size:10px;letter-spacing:.2em;color:var(--dim);margin-left:8px;text-transform:uppercase}
.docs{display:flex;gap:4px;flex-wrap:wrap}
.docs a{font-family:var(--mono);font-size:12px;letter-spacing:.06em;color:var(--dim);text-decoration:none;padding:6px 10px;border:1px solid transparent;border-radius:8px}
.docs a:hover{color:var(--fg)}
.docs a[aria-current="page"]{color:var(--fg);border-color:var(--line);background:var(--surf)}
.layout{max-width:1240px;margin:0 auto;padding-block:28px 72px;padding-left:max(16px,env(safe-area-inset-left,0px));padding-right:max(16px,env(safe-area-inset-right,0px));display:grid;grid-template-columns:270px minmax(0,1fr);gap:44px}
@media (max-width:900px){.layout{grid-template-columns:minmax(0,1fr);gap:16px}}
nav.toc{position:sticky;top:76px;align-self:start;max-height:calc(100vh - 96px);overflow:auto;font-size:14px;padding-right:12px}
@media (max-width:900px){nav.toc{position:static;max-height:none;padding-right:0}}
nav.toc details{border:1px solid var(--line);border-radius:12px;padding:10px 14px;background:var(--surf)}
nav.toc summary{cursor:pointer;font-family:var(--mono);font-size:11px;letter-spacing:.18em;text-transform:uppercase;color:var(--cyan)}
nav.toc ul{list-style:none;margin:8px 0 0;padding-left:0}
nav.toc ul ul{padding-left:12px;border-left:1px solid var(--line);margin:2px 0 6px 2px}
nav.toc a{color:var(--dim);text-decoration:none;display:block;padding:3px 0;line-height:1.35}
nav.toc a:hover,nav.toc a:focus-visible,nav.toc a.here{color:var(--fg)}
nav.toc a.here{box-shadow:inset 2px 0 0 var(--accent);padding-left:8px}
main{min-width:0;max-width:840px}
.hero{position:relative;padding:26px 26px 22px;margin:0 0 26px;border:1px solid var(--line);border-radius:16px;background:linear-gradient(180deg,rgba(184,112,255,.07),transparent 70%),var(--surf);overflow:hidden}
.hero::after{content:"";position:absolute;left:0;right:0;bottom:0;height:2px;background:var(--heat);opacity:.8}
.eyebrow{font-family:var(--mono);font-size:11px;letter-spacing:.22em;text-transform:uppercase;color:var(--cyan);margin:0 0 10px}
.hero h1{margin:0 0 10px}
.hero .lead{color:var(--mid);margin:0 0 16px;max-width:64ch}
.hero img.shot{display:block;width:100%;margin:6px 0 16px;border:1px solid var(--line);border-radius:10px}
.bar{display:flex;flex-wrap:wrap;gap:8px}
.btn{font-family:var(--body);font-weight:600;font-size:14px;text-decoration:none;color:var(--fg);border:1px solid var(--line);background:var(--raise);border-radius:10px;padding:8px 14px}
.btn.go{background:var(--fg);color:var(--bg);border-color:var(--fg)}
.btn:hover,.btn:focus-visible{border-color:var(--accent);outline:none}
h1,h2,h3,h4{font-family:var(--display);font-weight:400;line-height:1.2;text-wrap:balance;scroll-margin-top:84px;letter-spacing:.01em}
h1{font-size:clamp(26px,4.6vw,42px)}
h1 .fm{background:var(--heat);-webkit-background-clip:text;background-clip:text;color:transparent}
main>h1:first-of-type{display:none}
h2{font-size:clamp(19px,2.6vw,25px);margin:56px 0 14px;padding-top:18px;border-top:1px solid var(--line);position:relative}
h2::before{content:"";position:absolute;top:-1px;left:0;width:56px;height:2px;background:var(--heat)}
h3{font-size:17px;margin:34px 0 8px;color:var(--fg)}
h4{font-size:14px;margin:24px 0 6px;color:var(--mid);font-family:var(--mono);letter-spacing:.08em;text-transform:uppercase}
p,li{max-width:74ch}
li{margin:3px 0}
strong{color:#fff3ce;font-weight:600}
a{color:var(--vio);text-underline-offset:3px}
a:hover{color:var(--accent)}
code,kbd{font-family:var(--mono);font-size:.86em}
code{background:var(--raise);border:1px solid var(--line);border-radius:5px;padding:1px 5px;color:#fff3ce}
kbd{background:#16131f;border:1px solid rgba(255,122,26,.45);border-bottom-width:2px;border-radius:6px;padding:0 6px;white-space:nowrap;color:#ffd9b8}
.tw{overflow-x:auto;margin:14px 0;border:1px solid var(--line);border-radius:12px;background:var(--surf)}
table{border-collapse:collapse;width:100%;font-size:14px}
th,td{text-align:left;padding:9px 12px;border-bottom:1px solid var(--line);vertical-align:top}
tr:last-child td{border-bottom:0}
th{font-family:var(--mono);font-size:11px;letter-spacing:.12em;text-transform:uppercase;color:var(--cyan);font-weight:500;background:rgba(63,224,230,.04)}
tbody tr:hover td{background:rgba(184,112,255,.05)}
img{max-width:100%;height:auto;border-radius:10px}
pre{overflow-x:auto;background:var(--surf);border:1px solid var(--line);border-radius:12px;padding:14px 16px}
pre code{background:none;border:0;padding:0;color:var(--mid)}
blockquote{margin:14px 0;padding:10px 16px;border-left:3px solid var(--vio);background:rgba(184,112,255,.06);border-radius:0 10px 10px 0;color:var(--mid)}
blockquote p{margin:4px 0}
hr{border:0;border-top:1px solid var(--line);margin:36px 0}
.ess{position:relative;background:var(--surf);border:1px solid var(--line);border-radius:16px;padding:20px 22px;margin:8px 0 26px}
.ess h2{margin:0 0 8px;border:0;padding:0}
.ess h2::before{display:none}
.ess h3{font-size:12px;margin:18px 0 8px;color:var(--cyan);font-family:var(--mono);letter-spacing:.16em;text-transform:uppercase}
.ess dl{display:grid;grid-template-columns:minmax(0,260px) minmax(0,1fr);gap:8px 18px;margin:0}
.ess dt{min-width:0}
.ess dd{margin:0;min-width:0;color:var(--mid)}
@media (max-width:600px){.ess dl{grid-template-columns:minmax(0,1fr)}.ess dd{margin-bottom:8px}}
@media (max-width:640px){.top{position:static}nav.toc{top:12px}h1,h2,h3,h4{scroll-margin-top:12px}}
footer{max-width:1240px;margin:0 auto;padding:24px 16px 40px;color:var(--dim);font-size:13px;border-top:1px solid var(--line)}
footer a{color:var(--dim)}
@media print{.top,nav.toc,.bar{display:none}.layout{display:block}body{background:#fff;color:#000}}
"""

# every manual (the docs nav): key, page, label, the source; the ports' docs are in English
STUDIO = "https://juanjiant-bit.github.io/JIANT/studio/"
GH = "https://github.com/juanjiant-bit/JIANT/blob/main/"
PAGES = [
    ("fm", "manual.html", "JIANT FM · FM-1"),
    ("smk37", "smk37.html", "SMK-37"),
    ("mpc", "mpc-sample.html", "MPC Sample"),
    ("ports", "ports.html", "MPC plugin · ports"),
    ("plugin", "plugin.html", "VST3 · AU · LV2"),
]
# a source's .md the other pages are made from (links between them stay inside the manuals)
MD_PAGE = {"docs/MANUAL.md": "manual.html", "docs/MANUAL.es.md": "manual.es.html", "docs/SMK37.md": "smk37.html",
           "ports/mpc-sample/README.md": "mpc-sample.html", "ports/README.md": "ports.html",
           "ports/juce-plugin/README.md": "plugin.html"}
PORT_DOCS = {
    "smk37": dict(src="docs/SMK37.md", eyebrow="Manual · M-VAVE SMK-37 Pro / Elite", shot="smk37.webp",
                  title='JIANT <span class="fm">FM</span> on the SMK-37',
                  lead="JIANT bare metal on the SMK-37's own chip (the FM-1's AC791N): 37 keys, 16 RGB pads, 8 encoders and 4 faders as its panel, the same web editor and the same sounds. In development.",
                  links=[("Try it in the browser", "https://juanjiant-bit.github.io/JIANT/emu/devices.html#smk37"), ("JIANT Studio", STUDIO + "#ports")]),
    "mpc": dict(src="ports/mpc-sample/README.md", eyebrow="Manual · Akai MPC Sample", shot="mpc.webp",
                title='JIANT <span class="fm">FM</span> on the MPC Sample',
                lead="JIANT as the MPC Sample's whole instrument, started from the microSD card: its screen, pads, buttons and knobs as the FM-1's panel, with the SAMPLE and GRAIN engines, DRUM-X with samples and 16 sample slots. Preview: testers wanted.",
                links=[("Download", "https://github.com/juanjiant-bit/JIANT/releases"), ("Try it in the browser", "https://juanjiant-bit.github.io/JIANT/emu/devices.html#mpc"), ("JIANT Studio", STUDIO + "#ports")]),
    "ports": dict(src="ports/README.md", eyebrow="Manual · the ports and the MPC plugin (coming soon)", shot="mpc-plugin.webp",
                  title='JIANT <span class="fm">FM</span> beyond the FM-1',
                  lead="The same firmware from the same sources on other machines: the MPC plugin for the MPC standalones (MPC OS 3.10.1+) and desktop VST2 hosts, the MPC Sample app, the desktop plugins. A song sounds the same on every one.",
                  links=[("JIANT Studio", STUDIO + "#ports")]),
    "plugin": dict(src="ports/juce-plugin/README.md", eyebrow="Coming soon · VST3 · AU · AUv3 · LV2 · app", shot="plugin.webp",
                   title='JIANT <span class="fm">FM</span> in your DAW',
                   lead="For your DAW or the iPad (AUM, GarageBand, Logic, Cubasis…), with an editor made for a DAW: the live screen, the six tracks, each sound's presets and parameters, and a sample browser with drag and drop.",
                   links=[("JIANT Studio", STUDIO + "#ports")]),
}


def topbar(current, lang="en"):
    cur = ' aria-current="page"'
    docs = "".join(f'<a href="{href}"{cur if k == current else ""}>{html.escape(t)}</a>' for k, href, t in PAGES)
    word = "Manuales" if lang == "es" else "Manuals"
    return (f'<header class="top"><div class="in"><a class="brand" href="{STUDIO}">JIANT <b>FM</b><small>{word}</small></a>'
            f'<nav class="docs" aria-label="{word}">{docs}</nav></div></header>')


def fix_links(body, src_rel):
    """links to the other sources: to their pages; other relative links: to GitHub"""
    base = Path(src_rel).parent

    def rep(m):
        href = m.group(1)
        if re.match(r"^(https?:|mailto:|#|data:)", href):
            return m.group(0)
        path, _, frag = href.partition("#")
        target = os.path.normpath((base / path).as_posix()) if path else src_rel
        tail = "#" + frag if frag else ""
        if target in MD_PAGE:
            return f'href="{MD_PAGE[target]}{tail}"'
        return f'href="{GH}{target}{tail}"'
    return re.sub(r'href="([^"]+)"', rep, body)


SPY = """<script>
(function(){var d=document.querySelector('nav.toc details');if(d&&matchMedia('(max-width:900px)').matches)d.open=false;
var as=[].slice.call(document.querySelectorAll('nav.toc a[href^="#"]')),hs=as.map(function(a){return document.getElementById(decodeURIComponent(a.getAttribute('href').slice(1)))});
function spy(){var y=innerHeight*.25,k=-1;hs.forEach(function(h,i){if(h&&h.getBoundingClientRect().top<y)k=i});as.forEach(function(a,i){a.classList.toggle('here',i===k)})}
addEventListener('scroll',spy,{passive:true});spy();})();
</script>"""
FONTS = '<link rel="preconnect" href="https://fonts.googleapis.com"><link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Michroma&family=Instrument+Sans:wght@400;500;600&family=IBM+Plex+Mono:wght@400;500;600&display=swap">'


def inline_images(md_text, base):
    def rep(m):
        alt, src = m.group(1), m.group(2)
        p = (base / src).resolve()
        if p.is_file():
            mime = mimetypes.guess_type(p.name)[0] or "image/png"
            src = f"data:{mime};base64," + base64.b64encode(p.read_bytes()).decode()
        return f"![{alt}]({src})"
    return re.sub(r"!\[([^\]]*)\]\(([^)\s]+)\)", rep, md_text)


def essential(lang):
    title, groups = ESSENTIAL[lang]
    out = [f'<section class="ess" id="essential"><h2>{title}</h2>']
    for g, rows in groups:
        out.append(f"<h3>{g}</h3><dl>")
        for k, v in rows:
            out.append(f"<dt>{k}</dt><dd>{v}</dd>")
        out.append("</dl>")
    out.append("</section>")
    return "\n".join(out)


def page(lang, title, current, toc_title, toc, body, desc=""):
    return f"""<!doctype html>
<html lang="{lang}">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>{html.escape(title)}</title>
<meta name="description" content="{html.escape(desc)}">
<meta name="theme-color" content="#050409">
{FONTS}
<style>{CSS}</style>
</head>
<body>
{topbar(current, lang)}
<div class="layout">
<nav class="toc" aria-label="{toc_title}"><details open><summary>{toc_title}</summary>{toc}</details></nav>
<main>
{body}
</main>
</div>
<footer>JIANT FM, by Juan Jiant · GPL-3.0 · <a href="{STUDIO}">JIANT Studio</a> · <a href="https://github.com/juanjiant-bit/JIANT">GitHub</a>.
M-VAVE, FM-1, SMK-37, Akai and MPC are trademarks of their owners; JIANT is not affiliated with or endorsed by them.</footer>
{SPY}
</body>
</html>
"""


def convert(text, src_rel):
    md = markdown.Markdown(extensions=["tables", "toc", "fenced_code", "sane_lists"],
                           extension_configs={"toc": {"toc_depth": "2-3"}})
    body = md.convert(text)
    body = fix_links(body, src_rel)
    body = re.sub(r"<table>", '<div class="tw"><table>', body).replace("</table>", "</table></div>")
    body = body.replace(">MANUAL.es.md<", ">manual en español<").replace(">MANUAL.md<", ">English manual<")
    toc = md.toc.replace('<div class="toc">', "").replace("</div>", "")
    return body, toc


def data_uri(p):
    mime = mimetypes.guess_type(p.name)[0] or "image/png"
    return f"data:{mime};base64," + base64.b64encode(p.read_bytes()).decode()


def build(lang, src, dst):
    ui = UI[lang]
    text = inline_images(src.read_text(encoding="utf-8"), src.parent)
    body, toc = convert(text, src.relative_to(ROOT).as_posix())
    first_h2 = body.find("<h2")
    if first_h2 < 0:
        first_h2 = len(body)
    body = body[:first_h2] + essential(lang) + body[first_h2:]
    toc = toc.replace("<ul>", f'<ul><li><a href="#essential">{ESSENTIAL[lang][0]}</a></li>', 1)
    other, other_href = ui["other"]
    new_t, new_href = ui["new"]
    es = lang == "es"
    hero = f"""<section class="hero"><p class="eyebrow">{"Manual de uso · M-VAVE FM-1" if es else "User manual · M-VAVE FM-1"}</p>
<h1>JIANT <span class="fm">FM</span></h1>
<p class="lead">{"Todo JIANT FM en una página: cada botón, perilla y mensaje con el nombre que muestra el aparato. Si leés un solo capítulo, que sea <a href=\"#12-guardar-y-cargar\">Guardar y cargar</a>." if es else "All of JIANT FM on one page: every button, knob and message named as the device shows it. If you read one chapter, read <a href=\"#12-saving-and-loading\">Saving and loading</a>."}</p>
<div class="bar"><a class="btn go" href="{STUDIO}">{"Abrir JIANT Studio" if es else "Open JIANT Studio"}</a><a class="btn" href="{other_href}">{other}</a><a class="btn" href="{new_href}">{new_t}</a><a class="btn" href="https://juanjiant-bit.github.io/JIANT/manual/">{"Manual interactivo" if es else "Interactive manual"}</a><a class="btn" href="{ui['self']}" download>{ui['dl']}</a></div></section>
"""
    dst.write_text(page(lang, ui["title"], "fm", ui["toc"], toc, hero + body, ui["title"]), encoding="utf-8")
    print(dst.relative_to(ROOT), dst.stat().st_size)


def build_port(key, dst):
    d = PORT_DOCS[key]
    src = ROOT / d["src"]
    text = inline_images(src.read_text(encoding="utf-8"), src.parent)
    body, toc = convert(text, d["src"])
    shot = ROOT / "web" / "studio" / "img" / d["shot"]
    img = f'<img class="shot" src="{data_uri(shot)}" alt="">' if shot.is_file() else ""
    links = "".join(f'<a class="btn{" go" if n == 0 else ""}" href="{h}">{html.escape(t)}</a>' for n, (t, h) in enumerate(d["links"]))
    hero = f"""<section class="hero"><p class="eyebrow">{html.escape(d["eyebrow"])}</p><h1>{d["title"]}</h1>
<p class="lead">{html.escape(d["lead"])}</p>{img}<div class="bar">{links}<a class="btn" href="{GH}{d["src"]}">GitHub</a></div></section>
"""
    title = re.sub(r"<[^>]+>", "", d["title"])
    dst.write_text(page("en", title + " · Manual", key, "Contents", toc, hero + body, d["lead"]), encoding="utf-8")
    print(dst.relative_to(ROOT), dst.stat().st_size)


if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    build("en", DOCS / "MANUAL.md", OUT / "manual.html")
    build("es", DOCS / "MANUAL.es.md", OUT / "manual.es.html")
    for k, href, _ in PAGES[1:]:
        build_port(k, OUT / href)
