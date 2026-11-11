# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""tools/fetch_live_site.py against a small site served from this machine: the crawl finds every file, each way the
live site can be wrong stops it, and the artifact / live choice never goes back to an older site.

  python3 tests/test_fetch_live_site.py"""
import contextlib
import functools
import hashlib
import http.server
import importlib.util
import io
import json
import shutil
import tempfile
import threading
import unittest
from pathlib import Path

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('fetch_live_site', root/'tools/fetch_live_site.py')
fls = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fls)
fls.checkout_candidates = lambda: set()          # only what the served site itself tells


def package(seed):
    return (seed.encode()*64) + b'FELUCCA-LOADER-1'


def make_site(d, beta='V10.7.16-4815517-preview'):
    """the layout the workflows publish, in miniature"""
    def w(path, data):
        p = d/path
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data if isinstance(data, bytes) else data.encode())
    w('index.html', '<a href="manual/">m</a><img src="screens/home.webp"><script src="anim/add.js"></script>'
                    '<a href="https://juanjiant-bit.github.io/JIANT/emu/">e</a><a href="frecuencia.html">f</a>'
                    '<a href="https://example.com/x.png">out</a><a href="gone.html">dead</a>'
                    "<script>const L=['screens/' + n + '.webp'];</script>")
    w('frecuencia.html', '<a href="./">back</a>')
    w('404.html', '<p>404</p>')
    w('screens/home.webp', b'RIFF')
    w('anim/add.js', 'x=1')
    w('docs/cheatsheet-sound.pdf', b'%PDF')                      # linked nowhere: a known generator output
    w('emu/index.html', '<style>@font-face{src:url("fonts/DotGothic16-subset.woff")}</style>'
                        '<script>fetch("felucca.wasm");a.addModule("worklet.js?v="+Date.now())</script>')
    w('emu/worklet.js', 'w=1')
    w('emu/felucca.wasm', b'\0asm\1\0\0\0')
    w('emu/fonts/DotGothic16-subset.woff', b'wOFF')
    w('emu/SOURCE_COMMIT', '4'*40+'\n')
    w('manual/index.html', '<script src="manual.js"></script><script src="data.js"></script>')
    w('manual/manual.js', 'm=1')
    w('manual/data.js', "D=[{shot:'home'},{shots:['pg_a','pg_b']}]")
    for n in ('home', 'pg_a', 'pg_b'):
        w(f'manual/shots/{n}.png', b'\x89PNG\r\n\x1a\n'+n.encode())
    for c, versions in (('', ['V10.7.17', 'V10.7.16']), ('preview/', [beta, 'V10.7.15-aaaaaaa-preview'])):
        files = [f'felucca-{v}.fwsc' for v in versions]
        for f in files:
            w(c+'firmware/'+f, package(f))
        w(c+'firmware/versions.json', json.dumps([{'version': v, 'file': f} for v, f in zip(versions, files)]))
        w(c+'firmware/SHA256SUMS', ''.join(hashlib.sha256(package(f)).hexdigest()+'  '+f+'\n' for f in files))
        w(c+'firmware/SOURCE_COMMIT', '5'*40+'\n')
        w(c+'firmware/LICENSE', 'GPL')
        w(c+'firmware/LICENSING.md', 'md')
        w(c+'firmware/LICENSES/index.html', '<a href="Apache-2.0.txt">a</a><a href="../LICENSE">l</a>')
        w(c+'firmware/LICENSES/Apache-2.0.txt', 'apache')
        w(c+'webapp/installer/index.html', '<script>const META='+json.dumps(
            {'versions': [{'pkg': '../../firmware/'+f} for f in files]})+';</script><a href="../editor/">e</a>')
        w(c+'webapp/editor/index.html', '<style>@font-face{src:url(fukiai.ttf)}</style>')
        w(c+'webapp/editor/fukiai.ttf', b'ttf')
        w(c+'webapp/vel/index.html', 'vel')                      # linked nowhere


class Handler(http.server.SimpleHTTPRequestHandler):
    broken = set()                                               # paths answered with 503

    def do_GET(self):
        if self.path.split('?')[0] in self.broken:
            self.send_error(503)
            return
        super().do_GET()

    def log_message(self, *a):
        pass


def tree(d):
    return {p.relative_to(d).as_posix(): p.read_bytes() for p in Path(d).rglob('*') if p.is_file()}


class FetchLiveSite(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp())
        self.site = self.tmp/'www/JIANT'
        make_site(self.site)
        Handler.broken = set()
        self.httpd = http.server.ThreadingHTTPServer(
            ('127.0.0.1', 0), functools.partial(Handler, directory=str(self.tmp/'www')))
        threading.Thread(target=self.httpd.serve_forever, daemon=True).start()
        self.url = f'http://127.0.0.1:{self.httpd.server_address[1]}/JIANT/'

    def tearDown(self):
        self.httpd.shutdown()
        self.httpd.server_close()
        shutil.rmtree(self.tmp)

    def run_tool(self, *args):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = fls.main(list(args))
        return code, out.getvalue()

    def fetch(self, *extra):
        return self.run_tool('fetch', '--site', self.url, '--min-files', '20', '--retries', '1', *extra)

    def restore(self, *extra):
        return self.run_tool('restore', '--site', self.url, '--min-files', '20', '--retries', '1',
                             '--artifact', str(self.tmp/'released/artifact.tar'), *extra)

    def deploy(self, src=None):
        """what a deploy does: the manifest, then the tree goes live"""
        src = src or self.site
        code, out = self.run_tool('manifest', str(src), '--min-files', '20')
        self.assertEqual(code, 0, out)

    def test_crawl_recovers_every_file(self):
        code, out = self.fetch('--out', str(self.tmp/'mirror'))
        self.assertEqual(code, 0, out)
        self.assertEqual(tree(self.tmp/'mirror'), tree(self.site))
        self.assertIn('packages checked against SHA256SUMS: 4', out)
        self.assertIn('gone.html', out)                           # the dead link: reported, not fatal
        code, out = self.fetch('--dry-run', '--strict-links')
        self.assertEqual(code, 1, out)

    def test_dry_run_writes_nothing(self):
        code, out = self.fetch('--dry-run')
        self.assertEqual(code, 0, out)
        self.assertIn('nothing written', out)
        self.assertEqual(sorted(p.name for p in self.tmp.iterdir()), ['www'])

    def test_missing_listed_package_stops(self):
        (self.site/'firmware/felucca-V10.7.16.fwsc').unlink()
        code, out = self.fetch('--out', str(self.tmp/'mirror'))
        self.assertEqual(code, 1, out)
        self.assertIn('firmware/felucca-V10.7.16.fwsc: HTTP 404 (listed)', out)
        self.assertFalse((self.tmp/'mirror').exists())

    def test_package_not_matching_sha256sums_stops(self):
        (self.site/'preview/firmware/felucca-V10.7.15-aaaaaaa-preview.fwsc').write_bytes(package('other'))
        code, out = self.fetch('--dry-run')
        self.assertEqual(code, 1, out)
        self.assertIn('is not the', out)

    def test_server_error_stops_even_for_a_candidate(self):
        Handler.broken = {'/JIANT/webapp/btcheck/index.html'}
        code, out = self.fetch('--dry-run')
        self.assertEqual(code, 1, out)
        self.assertIn('HTTP 503', out)

    def test_missing_required_file_stops(self):
        (self.site/'emu/felucca.wasm').unlink()
        code, out = self.fetch('--dry-run')
        self.assertEqual(code, 1, out)
        self.assertIn('emu/felucca.wasm: HTTP 404 (required)', out)

    def test_installer_and_versions_from_different_deploys_stops(self):
        make_site(self.tmp/'other', beta='V10.7.18-513ed47-preview')
        shutil.copy(self.tmp/'other/preview/webapp/installer/index.html', self.site/'preview/webapp/installer/index.html')
        code, out = self.fetch('--dry-run')
        self.assertEqual(code, 1, out)
        self.assertIn('does not offer', out)

    def test_too_few_files_stops(self):
        code, out = self.run_tool('fetch', '--site', self.url, '--dry-run', '--retries', '1')
        self.assertEqual(code, 1, out)
        self.assertIn('fewer than --min-files', out)

    def test_manifest_fetch_is_exact_and_checked(self):
        (self.site/'secret/unlinked.bin').parent.mkdir()
        (self.site/'secret/unlinked.bin').write_bytes(b'only the manifest knows')
        self.deploy()
        code, out = self.fetch('--out', str(self.tmp/'mirror'))
        self.assertEqual(code, 0, out)
        self.assertIn('mode manifest', out)
        self.assertEqual(tree(self.tmp/'mirror'), tree(self.site))
        self.assertEqual(self.run_tool('verify', str(self.tmp/'mirror'))[0], 0)
        (self.site/'anim/add.js').write_text('x=2')              # one file of another deploy
        code, out = self.fetch('--dry-run')
        self.assertEqual(code, 1, out)
        self.assertIn('anim/add.js', out)

    def test_restore_missing_artifact_uses_live_and_later_steps_see_a_pages_tar(self):
        code, out = self.restore()
        self.assertEqual(code, 0, out)
        self.assertIn('the LIVE site', out)
        self.assertEqual(fls.read_tar(self.tmp/'released/artifact.tar'), tree(self.site))

    def test_restore_failure_leaves_no_tar(self):
        (self.site/'preview/firmware/versions.json').unlink()
        code, out = self.restore()
        self.assertEqual(code, 1, out)
        self.assertFalse((self.tmp/'released/artifact.tar').exists())

    def test_restore_keeps_a_current_artifact(self):
        self.deploy()
        fls.write_tar(tree(self.site), self.tmp/'released/artifact.tar')
        before = (self.tmp/'released/artifact.tar').read_bytes()
        code, out = self.restore()
        self.assertEqual(code, 0, out)
        self.assertIn('the downloaded artifact', out)
        self.assertEqual((self.tmp/'released/artifact.tar').read_bytes(), before)

    def test_restore_keeps_an_artifact_newer_than_a_cached_live_site(self):
        self.deploy()
        newer = self.tmp/'newer'
        shutil.copytree(self.site, newer)
        (newer/'anim/add.js').write_text('x=3')
        self.deploy(newer)
        m = json.loads((newer/'site-manifest.json').read_text())
        m['generated_epoch'] += 100
        (newer/'site-manifest.json').write_text(json.dumps(m))
        fls.write_tar(tree(newer), self.tmp/'released/artifact.tar')
        code, out = self.restore()
        self.assertEqual(code, 0, out)
        self.assertEqual(fls.read_tar(self.tmp/'released/artifact.tar')['anim/add.js'], b'x=3')

    def test_restore_never_rolls_back_to_a_stale_artifact(self):
        old = tree(self.site)                                     # an artifact from before the manifests
        (self.site/'anim/add.js').write_text('x=4')
        self.deploy()
        fls.write_tar(old, self.tmp/'released/artifact.tar')
        code, out = self.restore()
        self.assertEqual(code, 0, out)
        self.assertIn('OLDER than the live site', out)
        self.assertEqual(fls.read_tar(self.tmp/'released/artifact.tar'), tree(self.site))
        fls.write_tar(old, self.tmp/'released/artifact.tar')
        code, out = self.restore('--on-stale', 'fail')
        self.assertEqual(code, 1, out)

    def test_restore_artifact_without_live_manifest_is_the_old_behaviour(self):
        fls.write_tar(tree(self.site), self.tmp/'released/artifact.tar')
        code, out = self.restore()
        self.assertEqual(code, 0, out)
        self.assertIn('the downloaded artifact', out)

    def test_manifest_refuses_an_incomplete_site(self):
        (self.site/'firmware/felucca-V10.7.16.fwsc').unlink()
        code, out = self.run_tool('manifest', str(self.site), '--min-files', '20')
        self.assertEqual(code, 1, out)
        self.assertFalse((self.site/'site-manifest.json').exists())


if __name__ == '__main__':
    unittest.main()
