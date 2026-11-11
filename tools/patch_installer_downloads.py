#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Update published installer pages without rebuilding or changing firmware."""
from pathlib import Path
import hashlib
import sys

DOWNLOAD_WARN = (
    'Object.assign(TEXT.es, {downloadFwWarn:"Importante: instalá este archivo solo con el botón Install de esta página. '
    'Además de la app, el .fwsc trae la parte Bluetooth de la flash, donde viven GEN, el guardado de canciones y USB HOST. '
    'Otro flasher o el actualizador de M-VAVE pueden saltearla: el FM-1 arranca, pero GEN no anda y dice BLUETOOTH PART '
    'MISSING. Si te pasa, instalá de nuevo desde acá."});\n'
    'Object.assign(TEXT.en, {downloadFwWarn:"Important: install this file only with this page\'s Install button. Besides '
    'the app, the .fwsc carries the flash\'s Bluetooth part, where GEN, song saving and USB HOST live. Another flasher or '
    'M-VAVE\'s updater may skip it: the FM-1 starts, but GEN does not work and says BLUETOOTH PART MISSING. If that '
    'happens, install again from here."});\n'
    'Object.assign(TEXT.ja, {downloadFwWarn:"重要: このファイルはこのページの Install ボタンでのみインストールしてください。'
    '.fwsc にはアプリのほかにフラッシュの Bluetooth 部分が含まれ、GEN、ソングの保存、USB HOST はそこにあります。'
    '他のフラッシャーや M-VAVE のアップデーターはこの部分を書き込まないことがあり、FM-1 は起動しても GEN が動かず '
    'BLUETOOTH PART MISSING と表示されます。その場合はここからもう一度インストールしてください。"});\n'
)


def patch(html):
    def insert(old, new):
        nonlocal html
        if html.count(old) != 1:
            raise ValueError(f'Expected one installer anchor: {old[:60]}')
        html = html.replace(old, new, 1)
    if 'id="download-fw"' not in html:
        insert('  <p id="edition-info" class="small"></p>', '  <p id="edition-info" class="small"></p>\n  <p><a id="download-fw" download data-t="downloadFw" style="display:inline-block;padding:12px 16px;border:1px solid currentColor;text-decoration:none"></a></p>\n  <p class="small" data-t="downloadFwHelp"></p>')
        insert('const LANGS = ["es", "en", "ja"];', 'Object.assign(TEXT.es, {downloadFw:"Descargar firmware (.fwsc)",downloadFwHelp:"Descarga la versión seleccionada sin conectar el FM-1."});\nObject.assign(TEXT.en, {downloadFw:"Download firmware (.fwsc)",downloadFwHelp:"Download the selected version without connecting the FM-1."});\nObject.assign(TEXT.ja, {downloadFw:"ファームウェアをダウンロード (.fwsc)",downloadFwHelp:"FM-1 を接続せずに、選択したバージョンをダウンロードできます。"});\nconst LANGS = ["es", "en", "ja"];')
        insert('function showEdition() {\n  const v = versions[+$("ver").value] || versions[0];', 'function showEdition() {\n  const v = versions[+$("ver").value] || versions[0];\n  $("download-fw").href = v.pkg;\n  $("download-fw").download = v.pkg.split("/").pop();')
    if 'data-t="downloadFwWarn"' not in html:
        # The .fwsc carries bt.bin (the Bluetooth segment: GEN, song saving, USB HOST) as a third entry that only
        # JIANT's installer writes; another flasher leaves it out and GEN says BLUETOOTH PART MISSING.
        insert('<p class="small" data-t="downloadFwHelp"></p>', '<p class="small" data-t="downloadFwHelp"></p>\n  <p class="small" data-t="downloadFwWarn" style="color:#ec6e50"></p>')
        insert('const LANGS = ["es", "en", "ja"];', DOWNLOAD_WARN + 'const LANGS = ["es", "en", "ja"];')
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
