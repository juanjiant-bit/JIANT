#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""tools/patch_installer_downloads.py on installer pages as web/make_site.py builds them: from this checkout and from
earlier commits (the published pages were built from those: with the download button, without it), each as built and
as the previous version of the tool patched it. The result: the button gone, this checkout's flows, report and texts
in, the page's script still valid JavaScript (node --check), and a second patch changes nothing."""
from pathlib import Path
import json
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import patch_installer_downloads as P   # noqa: E402

LIBS = ('fm1pkg.js', 'fm1ota.js', 'fm1backup.js', 'fm1diag.js')
# e6bf12cd: the download button in index_pkg.html; 1d13ffc0: V10.7.21's site (button gone, before the Bluetooth check)
REVS = ('e6bf12cd', '1d13ffc0')
bad = 0


def ok(what, cond):
    global bad
    print(f'patch: {what:<92} {"ok" if cond else "FAIL"}')
    bad += not cond


def git_show(rev, path):
    r = subprocess.run(['git', '-C', str(ROOT), 'show', f'{rev}:{path}'], capture_output=True, text=True)
    return r.stdout if r.returncode == 0 else None


def built(read):
    """the page as web/make_site.py writes it (the libraries inlined, a version list, the brand's CSS)"""
    html = read('web/index_pkg.html')
    lib = '\n'.join(P.strip_module(read('web/' + n)) for n in LIBS)
    meta = json.dumps({'version': 'V10.7.21', 'product': 'FM-1_909', 'pkg': '../../firmware/felucca-V10.7.21.fwsc',
                       'channel': 'public', 'versions': [{'version': 'V10.7.21', 'pkg': '../../firmware/felucca-V10.7.21.fwsc',
                                                          'product': 'FM-1_909', 'edition': '', 'status': ''}]})
    css = read('web/jiant-ui.css') or ''
    return html.replace('/*LIB*/', lib).replace('/*META*/', meta).replace('/*JIANT_UI*/', css)


def script_ok(html, tmp):
    m = re.search(r'<script type="module">(.*)</script>', html, re.S)
    if not m or not shutil.which('node'):
        return m is not None
    f = Path(tmp) / 'page.mjs'
    f.write_text(m.group(1), encoding='utf-8')
    return subprocess.run(['node', '--check', str(f)], capture_output=True).returncode == 0


def main():
    src = P.sources()
    old_tool = git_show('751f95bd', 'tools/patch_installer_downloads.py')
    old_patch = None
    if old_tool:
        ns = {'__name__': 'old_patch_tool'}
        exec(compile(old_tool, 'old patch_installer_downloads.py', 'exec'), ns)
        old_patch = ns['patch']
    pages = [('this checkout', built(lambda p: (ROOT / p).read_text(encoding='utf-8')))]
    for rev in REVS:
        if git_show(rev, 'web/index_pkg.html') is None:
            print(f'patch: {rev} not in this clone (shallow): skipped')
            continue
        pages.append((rev, built(lambda p, r=rev: git_show(r, p))))
    with tempfile.TemporaryDirectory() as tmp:
        for name, html in pages:
            variants = [('as built', html)] + ([('as the previous tool patched it', old_patch(html))] if old_patch else [])
            for how, page in variants:
                out = P.patch(page, src)
                ok(f'{name}, {how}: patched (button gone, beta banner, flows / report / texts of this checkout)',
                   'download-fw' not in out and 'id="beta-banner"' in out and src['flows'] in out and src['report'] in out
                   and out.count(src['texts']) == 1 and out.index(P.TEXTS[0]) < out.index(P.LANGS)
                   and 'async function btCheck(' in out and 'ota.id.text !== JIANT_LOADER' in out)
                ok(f'{name}, {how}: patched again: unchanged', P.patch(out, src) == out)
                ok(f'{name}, {how}: the page script is valid JavaScript', script_ok(out, tmp))
        # the whole tool on a site tree: only the two installers change
        site = Path(tmp) / 'site'
        for d in ('webapp/installer', 'preview/webapp/installer', 'firmware'):
            (site / d).mkdir(parents=True)
        (site / 'webapp/installer/index.html').write_text(pages[-1][1], encoding='utf-8')
        (site / 'preview/webapp/installer/index.html').write_text(pages[0][1], encoding='utf-8')
        (site / 'firmware/versions.json').write_text('[]\n', encoding='utf-8')
        r = subprocess.run([sys.executable, str(ROOT / 'tools/patch_installer_downloads.py'), str(site)], capture_output=True, text=True)
        ok('the tool on a site tree: both installers updated, every other file unchanged',
           r.returncode == 0 and 'PASS' in r.stdout and
           all('function btCheck(' in (site / p).read_text(encoding='utf-8')
               for p in ('webapp/installer/index.html', 'preview/webapp/installer/index.html')))
    try:
        P.patch('<main></main><script type="module">function showChannel() {}</script>', src)
        ok('a page without the installer code: refused (the workflow stops, nothing published)', False)
    except ValueError:
        ok('a page without the installer code: refused (the workflow stops, nothing published)', True)
    print('patch test passed' if not bad else f'patch test: {bad} FAILED')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
