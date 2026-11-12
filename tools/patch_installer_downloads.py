#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Update published installer pages without rebuilding or changing firmware."""
from pathlib import Path
import hashlib
import re
import sys


def patch(html):
    """(V10.7.20) The firmware download button is gone: a .fwsc written by any other tool leaves the FM-1 without
    its Bluetooth part (GEN, song saving, USB HOST: "BLUETOOTH PART MISSING"); only this installer writes both.
    Removes the button in either form (this tool's earlier insert, index_pkg.html's own) and keeps the beta banner."""
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
    return html


def main(root):
    root = Path(root)
    paths = [root / p for p in ('webapp/installer/index.html', 'preview/webapp/installer/index.html')]
    def protected():
        return {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in root.rglob('*') if p.is_file() and p not in paths}
    before = protected()
    updates = [(p, patch(p.read_text())) for p in paths]
    for p, html in updates:
        p.write_text(html)
        print('Updated', p)
    assert protected() == before, 'Files outside the two installers changed'
    print('PASS: firmware, emulator, editors and all other site files unchanged')


if __name__ == '__main__':
    main(sys.argv[1])
