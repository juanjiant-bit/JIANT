#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""JIANT Studio on this computer, laid out as the published sites are (one origin, so the frames, the screen, the recorder
and the install safety work as they will on juanjiant-bit.github.io):

  /JIANT/studio/...           web/studio/
  /JIANT/webapp/editor/       web/editor.html (its brand style filled in as make_site.py does; fukiai.ttf beside it)
  /JIANT/emu/...              build/emu/ (web/emu/build.sh), else the live site's
  /JIANT-BYTE/webapp/editor/  ../JIANT-BYTE/web/editor.html, when that checkout is beside this one (or BYTE_DIR)
  /JIANT-BYTE/emu/...         ../JIANT-BYTE/build/emu/, else the live site's
  anything else               fetched from https://juanjiant-bit.github.io (the installers and their packages, ...)

  python3 web/studio/dev_server.py [PORT]     then open http://localhost:PORT/JIANT/studio/  (?mock=1: no FM-1)
"""
import os
import sys
import urllib.request
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

HERE = Path(__file__).resolve().parent
FM = HERE.parent.parent
BYTE = Path(os.environ.get("BYTE_DIR") or next((p for p in (FM.parent / "JIANT-BYTE", FM.parent / "jiant-byte") if p.exists()), FM.parent / "JIANT-BYTE"))
LIVE = "https://juanjiant-bit.github.io"
TYPES = {".html": "text/html; charset=utf-8", ".js": "text/javascript", ".mjs": "text/javascript", ".css": "text/css",
         ".wasm": "application/wasm", ".json": "application/json", ".ttf": "font/ttf", ".woff": "font/woff", ".png": "image/png",
         ".svg": "image/svg+xml", ".fwsc": "application/octet-stream", ".txt": "text/plain"}


def editor(repo):
    html = (repo / "web" / "editor.html").read_text(encoding="utf-8")
    brand = repo / "web" / "jiant-ui.css"
    return html.replace("/*JIANT_UI*/", brand.read_text(encoding="utf-8") if brand.exists() else "").encode()


def local(path):
    """the local file (bytes) for a site path, or None"""
    for site, repo in (("/JIANT/", FM), ("/JIANT-BYTE/", BYTE)):
        if not path.startswith(site):
            continue
        rest = path[len(site):]
        if rest in ("webapp/editor/", "webapp/editor/index.html") and (repo / "web" / "editor.html").exists():
            return editor(repo)
        if rest.startswith("webapp/editor/"):
            f = repo / "web" / rest[len("webapp/editor/"):]
            return f.read_bytes() if f.is_file() else None
        if site == "/JIANT/" and rest == "studio/fm1backup.js":       # (the installer's and the editor's, beside Studio)
            return (FM / "web" / "fm1backup.js").read_bytes()
        if site == "/JIANT/" and rest.startswith("studio/"):
            f = HERE / (rest[len("studio/"):] or "index.html")
            return f.read_bytes() if f.is_file() else None
        if rest.startswith("emu/"):
            f = repo / "build" / "emu" / (rest[len("emu/"):] or "index.html")
            return f.read_bytes() if f.is_file() else None
    return None


class Handler(SimpleHTTPRequestHandler):
    def do_GET(self):
        path = self.path.split("?")[0].split("#")[0]
        if path in ("/", "/JIANT", "/JIANT/studio"):
            self.send_response(302); self.send_header("Location", "/JIANT/studio/"); self.end_headers(); return
        body = local(path)
        kind = TYPES.get(Path(path).suffix or ".html", "application/octet-stream")
        if body is None:
            try:
                with urllib.request.urlopen(LIVE + self.path, timeout=30) as r:
                    body, kind = r.read(), r.headers.get("Content-Type", kind)
            except Exception as e:  # noqa: BLE001 (a missing page: say so)
                self.send_error(404, f"{path}: {e}"); return
        self.send_response(200)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *a):
        pass


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8770
    print(f"JIANT Studio: http://localhost:{port}/JIANT/studio/  (JIANT BYTE from {BYTE})")
    ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()
