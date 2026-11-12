#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""Update published installer pages without rebuilding or changing firmware.

Each published installer (webapp/installer/index.html, preview/webapp/installer/index.html) is web/index_pkg.html with
fm1pkg.js, fm1ota.js, fm1backup.js and fm1diag.js inlined (web/make_site.py). This tool brings the two pages up to this
checkout's installer code where it changed since they were built, from the sources themselves (never a copy kept here):

- (V10.7.20) the firmware download button removed, the beta banner kept;
- (V10.7.22) fm1diag.js's two flows (Install checks the Bluetooth part after the restart and writes it once more when
  it is missing; update mode with a loader that is not JIANT's is not resumed with JIANT) and its report, and
  index_pkg.html's block of their texts (es / en / ja).

Patching a page twice, or a page this tool already patched, changes nothing more (tests/patch_installer_test.py)."""
from pathlib import Path
import hashlib
import re
import sys

WEB = Path(__file__).resolve().parent.parent / 'web'
FLOWS = ('/* ------------------------------------------------------------------ the two flows --- */',
         '/* --------------------------------------------------------------- browser and MIDI --- */')
REPORT = ('/* ------------------------------------------------------------------- the report --- */',
          '  return lines.join("\\n");\n}\n')
TEXTS = ('/* (V10.7.22) the Bluetooth part: checked', "/* (V10.7.22) the Bluetooth part's texts end */\n")
LANGS = 'const LANGS = ["es", "en", "ja"];\n'


def strip_module(src):
    """as web/make_site.py inlines a module"""
    src = re.sub(r"^export\s+", "", src, flags=re.M)
    return re.sub(r"^import .*?;\n", "", src, flags=re.M)


def span(text, marks, what):
    """[start, end) of the part from marks[0] up to marks[1] (FLOWS: without it; the others: with it)"""
    a, b = marks
    if text.count(a) != 1:
        raise ValueError(f'{what}: expected one {a[:50]!r}, found {text.count(a)}')
    i = text.index(a)
    j = text.find(b, i)
    if j < 0:
        raise ValueError(f'{what}: no {b[:50]!r} after {a[:50]!r}')
    return i, (j if marks is FLOWS else j + len(b))


def sources():
    """the parts as this checkout has them (fm1diag.js inlined as make_site.py does, the texts' block as it is)"""
    diag = strip_module((WEB / 'fm1diag.js').read_text(encoding='utf-8'))
    page = (WEB / 'index_pkg.html').read_text(encoding='utf-8')
    out = {}
    for name, text, marks in (('flows', diag, FLOWS), ('report', diag, REPORT), ('texts', page, TEXTS)):
        i, j = span(text, marks, f'web/{"index_pkg.html" if name == "texts" else "fm1diag.js"}')
        out[name] = text[i:j]
    if not out['texts'].endswith(TEXTS[1]) or 'Object.assign(TEXT.ja' not in out['texts']:
        raise ValueError('web/index_pkg.html: the Bluetooth part texts block is incomplete')
    return out


def patch(html, src=None):
    """(V10.7.20) The firmware download button is gone: a .fwsc written by any other tool leaves the FM-1 without
    its Bluetooth part (GEN, song saving, USB HOST: "BLUETOOTH PART MISSING"); only this installer writes both.
    Removes the button in either form (this tool's earlier insert, index_pkg.html's own) and keeps the beta banner.
    (V10.7.22) The installer's flows, report and their texts as this checkout has them (sources())."""
    src = src or sources()

    def insert(old, new):
        nonlocal html
        if html.count(old) != 1:
            raise ValueError(f'Expected one installer anchor: {old[:60]}')
        html = html.replace(old, new, 1)
    for pat in (r'\n[ \t]*<p><a id="download-fw"[^\n]*?</a></p>',
                r'\n[ \t]*<p class="small" data-t="downloadFwHelp"></p>',
                r'\n[ \t]*\$\("download-fw"\)\.[^\n]*',
                r'\n[ \t]*#download-fw[^\n]*',
                r'\nObject\.assign\(TEXT\.(?:es|en|ja), ?\{ ?downloadFw:[^\n]*'):
        html = re.sub(pat, '', html)
    if 'download-fw' in html or 'downloadFw' in html:
        raise ValueError('Firmware download button still in the installer')
    if 'id="beta-banner"' not in html:
        insert('<main>', '<main>\n  <p id="beta-banner" hidden style="margin:0 0 20px;padding:14px;border:2px solid #ff8a00;color:#ff8a00;font:700 24px/1.2 monospace">BETA</p>')
        insert('function showChannel() {', 'function showChannel() {\n  $("beta-banner").hidden = !preview;')
    # (V10.7.22) the inlined fm1diag.js: its flows and its report replaced by this checkout's
    for name, marks in (('flows', FLOWS), ('report', REPORT)):
        i, j = span(html, marks, f'installer page ({name})')
        html = html[:i] + src[name] + html[j:]
    # .. and the texts' block: replaced where a page has it, else put right before LANGS (after every other text)
    if TEXTS[0] in html:
        i, j = span(html, TEXTS, 'installer page (texts)')
        html = html[:i] + src['texts'] + html[j:]
    else:
        insert(LANGS, src['texts'] + LANGS)
    if html.count('function btCheck(') != 1 or html.count('tip_foreignLoader') != 3 or html.index(TEXTS[0]) > html.index(LANGS):
        raise ValueError('The Bluetooth part check did not land in the installer')
    return html


def main(root):
    root = Path(root)
    paths = [root / p for p in ('webapp/installer/index.html', 'preview/webapp/installer/index.html')]

    def protected():
        return {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in root.rglob('*') if p.is_file() and p not in paths}
    before = protected()
    src = sources()
    updates = [(p, patch(p.read_text(encoding='utf-8'), src)) for p in paths]
    for p, html in updates:
        p.write_text(html, encoding='utf-8')
        print('Updated', p)
    assert protected() == before, 'Files outside the two installers changed'
    print('PASS: firmware, emulator, editors and all other site files unchanged')


if __name__ == '__main__':
    main(sys.argv[1])
