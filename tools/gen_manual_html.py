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
:root{--bg:#0b0d0c;--surf:#131816;--raise:#1a201d;--line:#26302b;--fg:#e9efeb;--mid:#a3b4aa;--dim:#6b7c72;--accent:#FF8A2A;
--display:"Bricolage Grotesque","Segoe UI",system-ui,sans-serif;--body:"IBM Plex Sans",system-ui,sans-serif;--mono:"IBM Plex Mono",ui-monospace,Menlo,monospace;color-scheme:dark}
@media (prefers-color-scheme: light){:root:not([data-theme="dark"]){--bg:#f1f4f2;--surf:#fff;--raise:#f6f8f7;--line:#d3ddd7;--fg:#141e19;--mid:#43564c;--dim:#6f8177;--accent:#c75a00;color-scheme:light}}
:root[data-theme="light"]{--bg:#f1f4f2;--surf:#fff;--raise:#f6f8f7;--line:#d3ddd7;--fg:#141e19;--mid:#43564c;--dim:#6f8177;--accent:#c75a00;color-scheme:light}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font-family:var(--body);font-size:16px;line-height:1.65}
.layout{max-width:1240px;margin:0 auto;padding-block:24px 64px;padding-left:max(16px,env(safe-area-inset-left,0px));padding-right:max(16px,env(safe-area-inset-right,0px));display:grid;grid-template-columns:270px minmax(0,1fr);gap:40px}
@media (max-width:900px){.layout{grid-template-columns:minmax(0,1fr);gap:16px}}
nav.toc{position:sticky;top:16px;align-self:start;max-height:calc(100vh - 32px);overflow:auto;font-size:14px;border-right:1px solid var(--line);padding-right:16px}
@media (max-width:900px){nav.toc{position:static;max-height:none;border-right:0;padding-right:0}}
nav.toc details{border:1px solid var(--line);border-radius:10px;padding:8px 12px;background:var(--surf)}
nav.toc summary{cursor:pointer;font-family:var(--mono);font-size:12px;letter-spacing:.1em;text-transform:uppercase;color:var(--mid)}
nav.toc ul{list-style:none;margin:6px 0 0;padding-left:0}
nav.toc ul ul{padding-left:12px}
nav.toc a{color:var(--mid);text-decoration:none;display:block;padding:2px 0}
nav.toc a:hover,nav.toc a:focus-visible{color:var(--fg)}
main{min-width:0;max-width:820px}
.bar{display:flex;flex-wrap:wrap;gap:8px;margin-bottom:18px}
.btn{font-family:var(--mono);font-weight:600;font-size:13px;text-decoration:none;color:var(--fg);border:1px solid var(--line);background:var(--surf);border-radius:999px;padding:6px 12px}
.btn:hover,.btn:focus-visible{border-color:var(--accent);outline:none}
h1,h2,h3,h4{font-family:var(--display);line-height:1.15;text-wrap:balance;scroll-margin-top:16px}
h1{font-size:clamp(32px,6vw,52px);margin:0 0 12px}
h2{font-size:28px;margin:48px 0 12px;padding-top:12px;border-top:1px solid var(--line)}
h3{font-size:20px;margin:32px 0 8px}
h4{font-size:17px;margin:24px 0 6px}
p,li{max-width:72ch}
a{color:var(--accent)}
code,kbd{font-family:var(--mono);font-size:.88em}
code{background:var(--raise);border:1px solid var(--line);border-radius:4px;padding:0 4px}
kbd{background:var(--raise);border:1px solid var(--line);border-bottom-width:2px;border-radius:5px;padding:0 5px;white-space:nowrap}
.tw{overflow-x:auto;margin:12px 0}
table{border-collapse:collapse;width:100%;font-size:14px}
th,td{text-align:left;padding:7px 10px;border-bottom:1px solid var(--line);vertical-align:top}
th{font-family:var(--mono);font-size:12px;letter-spacing:.06em;text-transform:uppercase;color:var(--dim);font-weight:600}
img{max-width:100%;height:auto;border-radius:8px}
pre{overflow-x:auto;background:var(--raise);border:1px solid var(--line);border-radius:8px;padding:12px}
blockquote{margin:12px 0;padding-left:14px;border-left:3px solid var(--accent);color:var(--mid)}
hr{border:0;border-top:1px solid var(--line);margin:32px 0}
.ess{background:var(--surf);border:1px solid var(--line);border-radius:14px;padding:18px 20px;margin:8px 0 24px}
.ess h2{margin:0 0 8px;border:0;padding:0}
.ess h3{font-size:16px;margin:16px 0 6px;color:var(--mid);font-family:var(--mono);letter-spacing:.06em;text-transform:uppercase}
.ess dl{display:grid;grid-template-columns:minmax(0,260px) minmax(0,1fr);gap:6px 16px;margin:0}
.ess dt{min-width:0}
.ess dd{margin:0;min-width:0}
@media (max-width:600px){.ess dl{grid-template-columns:minmax(0,1fr)}.ess dd{margin-bottom:8px;color:var(--mid)}}
"""


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


def build(lang, src, dst):
    ui = UI[lang]
    text = inline_images(src.read_text(encoding="utf-8"), src.parent)
    md = markdown.Markdown(extensions=["tables", "toc", "fenced_code", "sane_lists"],
                           extension_configs={"toc": {"toc_depth": "2-3"}})
    body = md.convert(text)
    body = body.replace('href="MANUAL.md"', 'href="manual.html"').replace('href="MANUAL.es.md"', 'href="manual.es.html"')
    body = re.sub(r"<table>", '<div class="tw"><table>', body).replace("</table>", "</table></div>")
    first_h2 = body.find("<h2")
    if first_h2 < 0:
        first_h2 = len(body)
    body = body[:first_h2] + essential(lang) + body[first_h2:]
    toc = md.toc.replace('<div class="toc">', "").replace("</div>", "")
    ess_link = f'<li><a href="#essential">{ESSENTIAL[lang][0]}</a></li>'
    toc = toc.replace("<ul>", "<ul>" + ess_link, 1)
    other, other_href = ui["other"]
    new_t, new_href = ui["new"]
    page = f"""<!doctype html>
<html lang="{lang}">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>{html.escape(ui['title'])}</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Bricolage+Grotesque:opsz,wght@12..96,600;12..96,800&family=IBM+Plex+Sans:wght@400;500;600&family=IBM+Plex+Mono:wght@500;600&display=swap">
<style>{CSS}</style>
</head>
<body>
<div class="layout">
<nav class="toc" aria-label="{ui['toc']}"><details open><summary>{ui['toc']}</summary>{toc}</details></nav>
<main>
<div class="bar"><a class="btn" href="{ui['self']}" download>{ui['dl']}</a><a class="btn" href="{other_href}">{other}</a><a class="btn" href="{new_href}">{new_t}</a><a class="btn" href="https://juanjiant-bit.github.io/JIANT/webapp/installer/">Installer</a><a class="btn" href="https://juanjiant-bit.github.io/JIANT/emu/">Emulator</a></div>
{body}
</main>
</div>
<script>
(function(){{var d=document.querySelector('nav.toc details');if(d&&matchMedia('(max-width:900px)').matches)d.open=false;}})();
</script>
</body>
</html>
"""
    dst.write_text(page, encoding="utf-8")
    print(dst.relative_to(ROOT), len(page))


if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    build("en", DOCS / "MANUAL.md", OUT / "manual.html")
    build("es", DOCS / "MANUAL.es.md", OUT / "manual.es.html")
