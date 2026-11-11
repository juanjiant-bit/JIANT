#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""The published site, recovered from the LIVE GitHub Pages site when the previous deploy's artifact is gone.

Every publishing workflow starts from the site the previous deploy uploaded (the `github-pages` artifact of a run),
changes one part of it and uploads the result. That artifact expires; the live site does not. This tool rebuilds the
tree from the live site, checks it, and refuses to hand over anything it could not verify.

  tools/fetch_live_site.py restore  [--artifact build/released/artifact.tar] [--site URL] [--work DIR]
      The step the workflows run after `actions/download-artifact` (continue-on-error). Leaves the site to start
      from at --artifact, in the Pages artifact's own format (a tar of the tree), so the steps after it do not change:
        the artifact is there and not older than the live site   -> kept as it is
        the artifact is missing (expired)                         -> the live site, fetched and packed
        the live site's manifest is newer than the artifact's     -> the live site (a stale run-id would roll the
                                                                     site back); --on-stale fail stops instead
  tools/fetch_live_site.py fetch    --out DIR [--tar FILE] [--site URL] [--dry-run]
      Only the mirror. --dry-run fetches and checks everything, prints the list, writes nothing.
  tools/fetch_live_site.py manifest DIR
      Writes DIR/site-manifest.json: every file of the site with its size and SHA-256. Every deploy runs it last, so
      the next recovery knows exactly which files exist and what their bytes must be.
  tools/fetch_live_site.py verify   DIR
      Checks DIR against its own site-manifest.json.

How the live site is enumerated:
  site-manifest.json on the live site (every deploy made with this tool)  -> exactly those files, each checked against
      its SHA-256 and size. One mismatch (for example the CDN still serving part of the previous deploy) stops it.
  no manifest yet (the first recovery)  -> a crawl: the files every generator writes (REQUIRED below), the packages
      named by firmware/versions.json and firmware/SHA256SUMS of both channels, every same-site link in the HTML, CSS
      and JSON fetched, the manual's screens as web/make_site.py finds them, and the candidates: paths found in
      scripts and the files of this checkout that the workflows copy to the site (a candidate that the live site does
      not have is not an error, it is listed).

It stops (exit 1, nothing written to --artifact / --tar) when: a required or listed file is not there, any request
ends in something other than 200 or 404 after the retries, a package does not match SHA256SUMS or is not a Felucca
package, an installer page does not offer the packages its versions.json lists, a text file comes back empty, or the
site has fewer files than --min-files. Python's standard library only.
"""
import argparse
import concurrent.futures
import datetime
import hashlib
import io
import json
import os
import posixpath
import re
import shutil
import sys
import tarfile
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

SITE = "https://juanjiant-bit.github.io/JIANT/"
MANIFEST = "site-manifest.json"
REPO = Path(__file__).resolve().parent.parent
CHANNELS = ("", "preview/")                       # the public installer and the beta one, the same layout
MIN_FILES = 120                                   # the manual alone is more than a hundred screens
MAX_FILES = 5000                                  # a crawl that finds more than this is not this site
PACKAGE = re.compile(r"felucca-[A-Za-z0-9.-]+\.fwsc")

# What every deploy of this site has, whichever workflow made it (web/make_site.py, .github/workflows/emulator.yml):
# one of these missing means the address is wrong, the site is down, or it is not the site these workflows publish.
REQUIRED = ["index.html", "404.html", "emu/index.html", "emu/worklet.js", "emu/felucca.wasm", "manual/index.html"] + [
    c + f for c in CHANNELS for f in (
        "webapp/installer/index.html", "webapp/editor/index.html", "firmware/versions.json", "firmware/LICENSE",
        "firmware/LICENSING.md", "firmware/LICENSES/index.html")]

# What a generator writes but no page may link (fetched when there, listed when not).
CANDIDATES = [
    "frecuencia.html", "app/jiant-probe.apk", "stems.js", "sample-bank.js",
    "emu/SOURCE_COMMIT", "emu/EDITION", "emu/stems.js", "emu/sample-bank.js", "emu/fm1backup.js",
    "emu/fonts/DotGothic16-subset.woff", "emu/fonts/OFL.txt",
    "preview/index.html", "preview/404.html",
    "docs/cheatsheet-controls.pdf", "docs/cheatsheet-sound.pdf", "docs/cheatsheet-controls.png",
    "docs/cheatsheet-sound.png", "docs/navigation-map.png", "docs/navigation-map.svg", "docs/navigation-map.es.png",
    "docs/navigation-map.es.svg",
] + ["manual/" + f for f in ("manual.js", "data.js", "nav.js", "sw.js", "tour.js", "tour.es.js", "tour.en.js",
                             "tour.pt.js", "tour.fr.js", "tour.ja.js", "tour.zh.js")] + [
    c + f for c in CHANNELS for f in (
        "firmware/SHA256SUMS", "firmware/SOURCE_COMMIT", "webapp/editor/fukiai.ttf",
        "webapp/editor/FUKIAI-LICENSE.txt", "webapp/editor/fm1backup.js", "webapp/btcheck/index.html",
        "webapp/vel/index.html")]

TEXT_EXT = (".html", ".htm", ".js", ".mjs", ".css", ".json", ".svg", ".md", ".txt")
ASSET_EXT = ("html", "htm", "js", "mjs", "css", "json", "png", "webp", "svg", "jpg", "jpeg", "gif", "ico", "pdf",
             "wasm", "woff", "woff2", "ttf", "otf", "txt", "md", "fwsc", "apk", "mp4", "webm", "wav", "mp3", "ogg",
             "bin", "map", "webmanifest", "xml")
ATTR = re.compile(r"""(?:\b(?:href|src|poster|action|data-src|data-href))\s*=\s*(?:"([^"]*)"|'([^']*)')""", re.I)
SRCSET = re.compile(r"""\bsrcset\s*=\s*(?:"([^"]*)"|'([^']*)')""", re.I)
CSS_URL = re.compile(r"""url\(\s*['"]?([^'")\s]+)['"]?\s*\)""", re.I)
REFRESH = re.compile(r"""http-equiv\s*=\s*["']?refresh["']?[^>]*?url\s*=\s*([^"'>\s]+)""", re.I)
SCRIPT = re.compile(r"<script\b[^>]*>(.*?)</script\s*>", re.I | re.S)
STRING = re.compile(r"""["'`]([^"'`\s<>{}()\\$,;|^*]{1,240})["'`]""")
HAS_EXT = re.compile(r"\.(?:%s)(?:[?#].*)?$" % "|".join(ASSET_EXT), re.I)


class Stop(Exception):
    """The site cannot be trusted as fetched: nothing is handed over."""


def log(*a):
    print(*a, flush=True)


def warn(msg):
    log(f"::warning::{msg}" if os.environ.get("GITHUB_ACTIONS") else f"WARNING: {msg}")


def sha256(data):
    return hashlib.sha256(data).hexdigest()


# ---------------------------------------------------------------------------------------------------------- HTTP

class Site:
    def __init__(self, base, retries=4, timeout=60, workers=8):
        self.base = base.rstrip("/") + "/"
        self.retries, self.timeout, self.workers = retries, timeout, workers
        self.requests = 0

    def get(self, path):
        """(status, bytes, final_path): 200 with the body, 404 without. Anything else, after the retries, stops the
        recovery: a file that may exist and could not be read must never be dropped from the site."""
        url = self.base + urllib.parse.quote(path, safe="/~!$&'()*+,;=:@%")
        last = None
        for attempt in range(self.retries):
            if attempt:
                time.sleep(min(30, 2 ** attempt))
            self.requests += 1
            try:
                req = urllib.request.Request(url, headers={"User-Agent": "fetch_live_site.py", "Accept": "*/*",
                                                           "Cache-Control": "no-cache"})
                with urllib.request.urlopen(req, timeout=self.timeout) as r:
                    data = r.read()
                    length = r.headers.get("Content-Length")
                    if length is not None and r.headers.get("Content-Encoding") in (None, "identity") \
                            and int(length) != len(data):
                        last = f"short read ({len(data)} of {length} B)"
                        continue
                    final = r.geturl()
                    if not final.startswith(self.base):
                        return 404, b"", path          # redirected off the site: not a file of it
                    return 200, data, urllib.parse.unquote(final[len(self.base):].split("?")[0].split("#")[0])
            except urllib.error.HTTPError as e:
                if e.code in (404, 410):
                    return 404, b"", path
                last = f"HTTP {e.code}"
            except Exception as e:                      # URLError, timeouts, resets, incomplete reads
                last = f"{type(e).__name__}: {e}"
        raise Stop(f"{url}: {last} after {self.retries} attempts")

    def get_many(self, paths):
        paths = list(paths)
        with concurrent.futures.ThreadPoolExecutor(self.workers) as pool:
            return dict(zip(paths, pool.map(self.get, paths)))


# ------------------------------------------------------------------------------------------------------- links

def normalise(base_dir, ref):
    """A reference found in a file that lives in base_dir -> the site path it names, or None when it leaves the site."""
    ref = ref.strip()
    if not ref or ref.startswith(("#", "data:", "mailto:", "javascript:", "blob:", "about:", "tel:")):
        return None
    ref = ref.split("#")[0].split("?")[0]
    if re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", ref) or ref.startswith("//"):
        u = urllib.parse.urlparse("https:" + ref if ref.startswith("//") else ref)
        for home in (SITE_URL, SITE):               # the address fetched, and the public one the pages spell out
            b = urllib.parse.urlparse(home)
            if u.netloc.lower() == b.netloc.lower() and (u.path + "/").startswith(b.path):
                path = u.path[len(b.path):]
                break
        else:
            return None
    elif ref.startswith("/"):
        root = urllib.parse.urlparse(SITE_URL).path           # "/JIANT/"
        if not (ref + "/").startswith(root):
            return None
        path = ref[len(root):]
    else:
        path = posixpath.join(base_dir, ref)
    try:
        path = urllib.parse.unquote(path)
    except Exception:
        return None
    directory = path == "" or path.endswith("/") or posixpath.basename(path) in (".", "..")
    path = posixpath.normpath(path.lstrip("/")) if path.strip("/") else ""
    if path.startswith("..") or any(ord(c) < 32 for c in path) or "\\" in path:
        return None
    if path in ("", "."):
        return "index.html"
    return path + "/index.html" if directory else path


def references(path, data):
    """(linked, candidates): what a fetched text file names. linked: markup that a browser follows (attributes, CSS
    url(), the redirect of a meta refresh). candidates: strings in scripts that look like a file of the site."""
    linked, cand = set(), set()
    if not path.lower().endswith(TEXT_EXT):
        return linked, cand
    text = data.decode("utf-8", "replace")
    here = posixpath.dirname(path)
    lower = path.lower()

    def add(target, ref, dirs=(here,)):
        ref = ref.strip()
        if not ref or re.search(r"""[\s"'`<>{}\\^|]|\$\{|^\+|\+$""", ref):
            return
        for d in dirs:
            p = normalise(d, ref)
            if p:
                target.add(p)

    markup = SCRIPT.sub(" ", text) if lower.endswith((".html", ".htm", ".svg")) else text
    if lower.endswith((".html", ".htm", ".svg")):
        for m in ATTR.finditer(markup):
            add(linked, m.group(1) if m.group(1) is not None else m.group(2))
        for m in SRCSET.finditer(markup):
            for part in (m.group(1) or m.group(2) or "").split(","):
                if part.strip():
                    add(linked, part.strip().split()[0])
        for m in REFRESH.finditer(markup):
            add(linked, m.group(1))
        for block in (m.group(1) for m in SCRIPT.finditer(text)):          # markup a script writes: only a candidate
            for m in ATTR.finditer(block):
                add(cand, m.group(1) if m.group(1) is not None else m.group(2))
    if lower.endswith((".html", ".htm", ".css", ".svg")):
        for m in CSS_URL.finditer(markup):
            add(linked, m.group(1))
    if lower.endswith((".html", ".htm", ".js", ".mjs", ".json")):
        # a script's relative paths resolve against the page that runs it: its own folder, the folder above (the
        # front page runs anim/*.js) and the site root cover every page of this site
        dirs = {here, posixpath.dirname(here), ""}
        for m in STRING.finditer(text):
            s = m.group(1)
            if HAS_EXT.search(s) and not s.startswith(("data:", "blob:")) and len(s) > 2:
                add(cand, s, dirs)
    return linked - {path}, cand - linked - {path}


def manual_screens(files):
    """manual/shots/NAME.png for every screen the live manual's scripts name: web/make_site.py manual()'s own search"""
    names = set()
    for f in ("nav.js", "data.js", "tour.js"):
        text = files.get("manual/" + f, b"").decode("utf-8", "replace")
        names |= set(re.findall(r"""(?:"shot":"|\bshot:\s*')([\w-]+)""", text))
        if f == "data.js":
            for m in re.findall(r"\bshots:\s*\[([^\]]*)\]", text):
                names |= set(re.findall(r"'([\w-]+)'", m))
    return {f"manual/shots/{n}.png" for n in names}


def checkout_candidates():
    """The files of this checkout that .github/workflows/emulator.yml copies to the site by folder: the live site was
    built from an earlier commit, so each is only a candidate."""
    out = set()
    for src, dst in (("web/landing/anim", "anim"), ("web/landing/screens", "screens"),
                     ("web/manual/shots", "manual/shots"), ("web/emu/fonts", "emu/fonts")):
        d = REPO / src
        if d.is_dir():
            out |= {f"{dst}/{p.relative_to(d).as_posix()}" for p in d.rglob("*") if p.is_file()}
    lic = REPO / "LICENSES"
    if lic.is_dir():
        out |= {f"{c}firmware/LICENSES/{p.name}" for c in CHANNELS for p in lic.glob("*.txt")}
    return out


# ------------------------------------------------------------------------------------------------- enumeration

def packages_of(files, channel, problems):
    """{package: sha256 or None} for one channel, from versions.json and SHA256SUMS; the checks both must pass"""
    fw = channel + "firmware/"
    want = {}
    try:
        listed = json.loads(files[fw + "versions.json"].decode("utf-8"))
        assert isinstance(listed, list) and listed
    except Exception as e:
        problems.append(f"{fw}versions.json: not a list of versions ({e})")
        return want
    for e in listed:
        f = str(e.get("file", "")) if isinstance(e, dict) else ""
        if not PACKAGE.fullmatch(f):
            problems.append(f"{fw}versions.json: unexpected package name {f!r}")
            continue
        want[fw + f] = None
    sums = files.get(fw + "SHA256SUMS")
    if sums is None:
        warn(f"{fw}SHA256SUMS is not on the live site: its packages cannot be checked against a digest")
        return want
    listed_names = {posixpath.basename(p) for p in want}
    for line in sums.decode("utf-8", "replace").splitlines():
        m = re.fullmatch(r"([0-9a-f]{64})\s+\*?(\S+)", line.strip())
        if not m:
            if line.strip():
                problems.append(f"{fw}SHA256SUMS: unreadable line {line!r}")
            continue
        if not PACKAGE.fullmatch(m.group(2)):
            problems.append(f"{fw}SHA256SUMS: unexpected name {m.group(2)!r}")
            continue
        want[fw + m.group(2)] = m.group(1)
    for n in sorted(listed_names):
        if want.get(fw + n) is None:
            problems.append(f"{fw}SHA256SUMS has no digest for {n}, which versions.json lists")
    return want


def crawl(site, strict_links=False):
    """The live site without a manifest -> ({path: bytes}, report)"""
    files, missing, origin = {}, {}, {}
    problems = []

    def want(paths, kind):
        new = []
        rank = {"required": 0, "listed": 1, "linked": 2, "candidate": 3}
        for p in sorted(paths):
            if p == MANIFEST:
                continue
            if p not in origin or rank[kind] < rank[origin[p]]:
                origin[p] = kind
            if p not in files and p not in missing and p not in new:
                new.append(p)
        return new

    def fetch(paths):
        if len(files) + len(paths) > MAX_FILES:
            raise Stop(f"more than {MAX_FILES} files referenced: this is not the site this tool knows")
        got = site.get_many(paths)
        fresh = []
        for p, (status, data, final) in got.items():
            if status != 200:
                missing[p] = status
                continue
            if final != p:
                # Pages redirects "folder" to "folder/": the file is the folder's index.html
                real = final + "index.html" if final.endswith("/") or final == "" else final
                origin.setdefault(real, origin.get(p, "candidate"))
                origin.pop(p, None)
                if real in files:
                    continue
                p = real
            files[p] = data
            fresh.append(p)
        return fresh

    queue = want(REQUIRED, "required") + want(CANDIDATES, "candidate") + want(checkout_candidates(), "candidate")
    seen_channels = False
    while queue:
        fresh = fetch(queue)
        queue = []
        for p in fresh:
            linked, cand = references(p, files[p])
            queue += want(linked, "linked") + want(cand, "candidate")
        if not seen_channels and all(c + "firmware/versions.json" in files for c in CHANNELS):
            seen_channels = True
            for c in CHANNELS:
                queue += want(packages_of(files, c, problems), "listed")
        queue += want(manual_screens(files), "listed")
        queue = sorted(set(queue))

    # Pages also answers "name" with name.html's bytes: keep the real file only
    for p in [p for p in files if "." not in posixpath.basename(p) and p + ".html" in files
              and files[p] == files[p + ".html"]]:
        del files[p]
        origin.pop(p, None)

    for p, status in sorted(missing.items()):
        kind = origin.get(p, "candidate")
        if kind in ("required", "listed") or (kind == "linked" and strict_links):
            problems.append(f"{p}: HTTP {status} ({kind})")
    digests = {}
    for c in CHANNELS:
        if c + "firmware/versions.json" in files:
            digests.update(packages_of(files, c, []))
    check_site(files, digests, problems)
    report = {"mode": "crawl", "origin": origin, "missing": missing, "problems": problems, "digests": digests}
    return files, report


def check_site(files, digests, problems):
    """What must hold for any mirror of this site, however it was enumerated"""
    for p in sorted(files):
        data = files[p]
        if p.endswith(".fwsc"):
            want = digests.get(p)
            if want and sha256(data) != want:
                problems.append(f"{p}: SHA-256 {sha256(data)} is not the {want} its SHA256SUMS lists")
            if b"FELUCCA-LOADER-1" not in data:
                problems.append(f"{p}: not a Felucca package (no loader marker), {len(data)} B")
        elif p.lower().endswith((".html", ".js", ".json", ".css", ".wasm", ".ttf", ".pdf", ".png", ".webp")) \
                and not data:
            problems.append(f"{p}: empty")
        if p.endswith(".wasm") and data[:4] != b"\0asm":
            problems.append(f"{p}: not WebAssembly")
        if p.endswith(".png") and data[:8] != b"\x89PNG\r\n\x1a\n":
            problems.append(f"{p}: not a PNG")
    for c in CHANNELS:
        fw, page = c + "firmware/", c + "webapp/installer/index.html"
        try:
            listed = [str(e["file"]) for e in json.loads(files[fw + "versions.json"].decode("utf-8"))]
        except Exception:
            problems.append(f"{fw}versions.json: missing or unreadable")
            continue
        html = files.get(page, b"").decode("utf-8", "replace")
        for f in listed:
            if fw + f not in files:
                problems.append(f"{fw}{f}: listed by versions.json, not fetched")
            if f'../../firmware/{f}"' not in html:
                problems.append(f"{page} does not offer {f}, which {fw}versions.json lists: the site is being "
                                "served half from one deploy and half from another, try again later")
    for p in REQUIRED:
        if p not in files:
            problems.append(f"{p}: required, not fetched")


def from_manifest(site, manifest):
    """The live site with a manifest: exactly its files, each checked -> ({path: bytes}, report)"""
    entries = manifest["files"]
    if len(entries) > MAX_FILES:
        raise Stop(f"{MANIFEST} lists {len(entries)} files")
    for p in entries:
        if normalise("", p) != p or p == MANIFEST:
            raise Stop(f"{MANIFEST} lists an unsafe path: {p!r}")
    got = site.get_many(sorted(entries))
    files, problems = {}, []
    for p, (status, data, final) in sorted(got.items()):
        e = entries[p]
        if status != 200:
            problems.append(f"{p}: HTTP {status}, the manifest lists it")
        elif len(data) != e["size"] or sha256(data) != e["sha256"]:
            problems.append(f"{p}: {len(data)} B sha256 {sha256(data)}, the manifest lists {e['size']} B "
                            f"{e['sha256']} (a deploy still spreading? try again in ten minutes)")
        else:
            files[p] = data
    digests = {}
    for c in CHANNELS:
        if c + "firmware/versions.json" in files:
            digests.update(packages_of(files, c, problems))
    check_site(files, digests, problems)
    files[MANIFEST] = (json.dumps(manifest, indent=1, sort_keys=True) + "\n").encode()
    report = {"mode": "manifest", "origin": {p: "manifest" for p in files}, "missing": {}, "problems": problems,
              "digests": digests}
    return files, report


def live_manifest(site):
    """The live site's manifest, None when it has none; unreadable is an error (never guess)"""
    status, data, _ = site.get(MANIFEST)
    if status != 200:
        return None
    try:
        m = json.loads(data.decode("utf-8"))
        assert m["format"] == 1 and isinstance(m["files"], dict) and m["files"]
        for p, e in m["files"].items():
            assert isinstance(p, str) and re.fullmatch(r"[0-9a-f]{64}", e["sha256"]) and isinstance(e["size"], int)
        float(m["generated_epoch"])
        return m
    except Exception as e:
        raise Stop(f"{site.base}{MANIFEST} is not a manifest this tool wrote: {e}")


def mirror(site, min_files, strict_links=False, manifest="auto"):
    m = live_manifest(site) if manifest == "auto" else manifest
    if m:
        log(f"live site: {MANIFEST} of {m.get('generated_at')} (run {m.get('run_id')}, {len(m['files'])} files): "
            "fetching exactly those")
        files, report = from_manifest(site, m)
    else:
        log(f"live site: no {MANIFEST}: enumerating by crawl, versions.json, SHA256SUMS and the generators' outputs")
        files, report = crawl(site, strict_links)
    if len(files) < min_files:
        report["problems"].append(f"only {len(files)} files, fewer than --min-files {min_files}")
    print_report(site, files, report)
    if report["problems"]:
        raise Stop(f"{len(report['problems'])} problem(s); the site was NOT recovered:\n  "
                   + "\n  ".join(report["problems"]))
    return files, report


def print_report(site, files, report):
    origin = report["origin"]
    log(f"\n=== {site.base}: {len(files)} files, {sum(map(len, files.values()))} B, {site.requests} requests, "
        f"mode {report['mode']} ===")
    log(f"{'sha256':64}  {'bytes':>9}  how found   path")
    for p in sorted(files):
        log(f"{sha256(files[p])}  {len(files[p]):9d}  {origin.get(p, '?'):10}  {p}")
    by_top = {}
    for p in files:
        top = p.split("/")[0] if "/" in p else "(root)"
        by_top[top] = by_top.get(top, 0) + 1
    log("\nper folder: " + ", ".join(f"{k} {v}" for k, v in sorted(by_top.items())))
    for c in CHANNELS:
        try:
            v = json.loads(files[c + "firmware/versions.json"].decode())
            log(f"{c or 'public/'}firmware: " + " ".join(str(e.get("version")) for e in v))
        except Exception:
            pass
    checked = [p for p, d in report["digests"].items() if d and p in files]
    log(f"packages checked against SHA256SUMS: {len(checked)}")
    linked404 = sorted(p for p, s in report["missing"].items() if origin.get(p) == "linked")
    cand404 = sorted(p for p, s in report["missing"].items() if origin.get(p, "candidate") == "candidate")
    if linked404:
        log(f"\nlinked from a page but not on the live site ({len(linked404)}; dead links, nothing to recover):")
        for p in linked404:
            log(f"  {p}")
            warn(f"live site links {p}, which it does not have")
    if cand404:
        log(f"\ncandidates the live site does not have ({len(cand404)}; not an error):")
        for p in cand404:
            log(f"  {p}")
    for x in report["problems"]:
        log(f"::error::{x}" if os.environ.get("GITHUB_ACTIONS") else f"ERROR: {x}")


# ------------------------------------------------------------------------------------------------ tree / tar

def safe_target(root, rel):
    target = (root / rel).resolve()
    if root.resolve() not in target.parents:
        raise Stop(f"unsafe path {rel!r}")
    return target


def write_tree(files, out):
    out = Path(out)
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    for p in sorted(files):
        t = safe_target(out, p)
        t.parent.mkdir(parents=True, exist_ok=True)
        t.write_bytes(files[p])


def write_tar(files, tar_path):
    """As actions/upload-pages-artifact packs it: the tree at the tar's root"""
    tar_path = Path(tar_path)
    tar_path.parent.mkdir(parents=True, exist_ok=True)
    tmp = tar_path.with_name(tar_path.name + ".tmp")
    with tarfile.open(tmp, "w") as tar:
        for p in sorted(files):
            info = tarfile.TarInfo("./" + p)
            info.size, info.mode, info.mtime = len(files[p]), 0o644, int(time.time())
            tar.addfile(info, io.BytesIO(files[p]))
    os.replace(tmp, tar_path)


def read_tar(tar_path):
    files = {}
    with tarfile.open(tar_path) as tar:
        for info in tar:
            if not info.isfile():
                continue
            p = posixpath.normpath(info.name).lstrip("/")
            if p.startswith(".."):
                raise Stop(f"{tar_path}: unsafe member {info.name!r}")
            files[p] = tar.extractfile(info).read()
    return files


def build_manifest(files):
    now = time.time()
    env = os.environ
    return {
        "format": 1,
        "generated_at": datetime.datetime.fromtimestamp(now, datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "generated_epoch": int(now),
        "run_id": env.get("GITHUB_RUN_ID", ""), "run_attempt": env.get("GITHUB_RUN_ATTEMPT", ""),
        "workflow": env.get("GITHUB_WORKFLOW", ""), "commit": env.get("GITHUB_SHA", ""),
        "count": len(files), "bytes": sum(map(len, files.values())),
        "files": {p: {"sha256": sha256(files[p]), "size": len(files[p])} for p in sorted(files)},
    }


def read_tree(root):
    root = Path(root)
    files = {}
    for p in sorted(root.rglob("*")):
        if p.is_symlink():
            raise Stop(f"{p}: a symbolic link in the site")
        if p.is_file():
            files[p.relative_to(root).as_posix()] = p.read_bytes()
    return files


def verify_against(files, manifest, what):
    listed = manifest["files"]
    have = {p: d for p, d in files.items() if p != MANIFEST}
    bad = [f"{p}: not in the manifest" for p in sorted(set(have) - set(listed))]
    bad += [f"{p}: in the manifest, not in the site" for p in sorted(set(listed) - set(have))]
    bad += [f"{p}: bytes differ from the manifest" for p in sorted(set(listed) & set(have))
            if sha256(have[p]) != listed[p]["sha256"] or len(have[p]) != listed[p]["size"]]
    if bad:
        raise Stop(f"{what} does not match its own {MANIFEST}:\n  " + "\n  ".join(bad[:40]))


# ------------------------------------------------------------------------------------------------- commands

def cmd_manifest(a):
    root = Path(a.dir)
    files = read_tree(root)
    files.pop(MANIFEST, None)
    if len(files) < a.min_files:
        raise Stop(f"{root}: only {len(files)} files, fewer than --min-files {a.min_files}: not publishing this")
    problems = []
    digests = {}
    for c in CHANNELS:
        digests.update(packages_of(files, c, problems) if c + "firmware/versions.json" in files else {})
    check_site(files, digests, problems)
    if problems:
        raise Stop(f"{root} is not a complete site:\n  " + "\n  ".join(problems))
    m = build_manifest(files)
    (root / MANIFEST).write_text(json.dumps(m, indent=1, sort_keys=True) + "\n", encoding="utf-8")
    for p, e in m["files"].items():
        log(f"{e['sha256']}  {e['size']:9d}  {p}")
    log(f"{root / MANIFEST}: {m['count']} files, {m['bytes']} B")


def cmd_verify(a):
    files = read_tree(a.dir)
    if MANIFEST not in files:
        raise Stop(f"{a.dir}: no {MANIFEST}")
    verify_against(files, json.loads(files[MANIFEST]), a.dir)
    log(f"{a.dir}: {len(files) - 1} files match {MANIFEST}")


def cmd_fetch(a):
    site = Site(a.site, a.retries, a.timeout, a.workers)
    files, report = mirror(site, a.min_files, a.strict_links)
    if a.dry_run:
        log(f"\ndry run: {len(files)} files fetched and checked, nothing written")
        return
    if a.out:
        write_tree(files, a.out)
        log(f"\nwritten: {a.out} ({len(files)} files)")
    if a.tar:
        write_tar(files, a.tar)
        log(f"written: {a.tar}")


def output(key, value):
    if os.environ.get("GITHUB_OUTPUT"):
        with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as f:
            f.write(f"{key}={value}\n")


def summary(text):
    log(text)
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as f:
            f.write(text + "\n\n")


def cmd_restore(a):
    artifact = Path(a.artifact)
    site = Site(a.site, a.retries, a.timeout, a.workers)
    have, a_manifest = None, None
    if artifact.is_file() and artifact.stat().st_size:
        have = read_tar(artifact)
        if len(have) < a.min_files:
            raise Stop(f"{artifact}: only {len(have)} files, fewer than --min-files {a.min_files}")
        if MANIFEST in have:
            a_manifest = json.loads(have[MANIFEST])
            verify_against(have, a_manifest, str(artifact))
        log(f"artifact: {artifact}, {len(have)} files"
            + (f", manifest of {a_manifest['generated_at']} (run {a_manifest.get('run_id')})" if a_manifest
               else f", no {MANIFEST} (made before this tool)"))
    else:
        log(f"artifact: {artifact} is not there (expired, or the download failed)")
    try:
        l_manifest = live_manifest(site)
    except Stop as e:
        if have is None:
            raise
        warn(f"the live site's manifest could not be read ({e}); the downloaded artifact is used as it is")
        l_manifest = "unknown"
    if have is not None:
        stale = isinstance(l_manifest, dict) and (
            a_manifest is None or float(l_manifest["generated_epoch"]) > float(a_manifest["generated_epoch"]))
        if not stale:
            summary(f"Site to start from: the downloaded artifact ({len(have)} files)"
                    + ("" if isinstance(l_manifest, dict) or l_manifest == "unknown" else
                       f"; the live site has no {MANIFEST} yet, so it was not compared"))
            output("source", "artifact")
            return
        msg = (f"the downloaded artifact ({a_manifest['generated_at'] + ', run ' + str(a_manifest.get('run_id')) if a_manifest else 'no manifest'}) "
               f"is OLDER than the live site ({l_manifest['generated_at']}, run {l_manifest.get('run_id')}): "
               "the workflow's run-id is stale and would roll the site back")
        if a.on_stale == "fail":
            raise Stop(msg + "; update the run-id")
        warn(msg + "; starting from the live site instead")
    files, report = mirror(site, a.min_files, a.strict_links,
                           manifest=l_manifest if isinstance(l_manifest, dict) else "auto")
    if a.work:
        write_tree(files, a.work)
    write_tar(files, artifact)
    summary(f"Site to start from: the LIVE site {site.base} ({len(files)} files, mode {report['mode']}), packed as "
            f"{artifact}")
    output("source", "live-" + report["mode"])


def main(argv=None):
    global SITE_URL
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0], formatter_class=argparse.RawTextHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    def net(p):
        p.add_argument("--site", default=os.environ.get("JIANT_SITE_URL", SITE), help=f"default {SITE}")
        p.add_argument("--min-files", type=int, default=MIN_FILES)
        p.add_argument("--strict-links", action="store_true", help="a dead link on the live site stops it too")
        p.add_argument("--retries", type=int, default=4)
        p.add_argument("--timeout", type=int, default=60)
        p.add_argument("--workers", type=int, default=8)

    p = sub.add_parser("restore", help="the artifact, or the live site when it is missing or stale")
    net(p)
    p.add_argument("--artifact", default="build/released/artifact.tar")
    p.add_argument("--work", default="", help="also leave the fetched tree here")
    p.add_argument("--on-stale", choices=("live", "fail"), default="live")
    p.set_defaults(fn=cmd_restore)
    p = sub.add_parser("fetch", help="mirror the live site")
    net(p)
    p.add_argument("--out", default="")
    p.add_argument("--tar", default="")
    p.add_argument("--dry-run", action="store_true")
    p.set_defaults(fn=cmd_fetch)
    p = sub.add_parser("manifest", help="write DIR/site-manifest.json")
    p.add_argument("dir")
    p.add_argument("--min-files", type=int, default=MIN_FILES)
    p.set_defaults(fn=cmd_manifest)
    p = sub.add_parser("verify", help="check DIR against its site-manifest.json")
    p.add_argument("dir")
    p.set_defaults(fn=cmd_verify)
    a = ap.parse_args(argv)
    SITE_URL = (getattr(a, "site", SITE)).rstrip("/") + "/"
    if a.cmd == "fetch" and not (a.dry_run or a.out or a.tar):
        ap.error("fetch needs --out, --tar or --dry-run")
    try:
        a.fn(a)
    except Stop as e:
        log(f"::error::{str(e).splitlines()[0]}" if os.environ.get("GITHUB_ACTIONS") else "")
        log(f"\nSTOP: {e}")
        return 1
    return 0


SITE_URL = SITE

if __name__ == "__main__":
    sys.exit(main())
