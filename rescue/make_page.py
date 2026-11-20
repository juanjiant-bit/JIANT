#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Make rescue/fm1-rescue.html: page.template.html with builder.js and the two
base firmware images (rescue/base/*.uf2) inlined, so the page works offline,
opened as a local file or from any static host.

  python3 rescue/make_page.py
"""
import base64
import re
from pathlib import Path

HERE = Path(__file__).resolve().parent


def main():
    page = (HERE / "page.template.html").read_text(encoding="utf-8")
    builder = (HERE / "builder.js").read_text(encoding="utf-8")
    builder = re.sub(r"^export ", "", builder, flags=re.M)
    for board in ("zero", "xiao"):
        uf2 = (HERE / "base" / ("fm1-rescue-base-%s.uf2" % board)).read_bytes()
        if len(uf2) % 512 or not uf2:
            raise SystemExit("base/%s: not a UF2" % board)
        page = page.replace("%%%%BASE_%s%%%%" % board.upper(), base64.b64encode(uf2).decode())
    page = page.replace("%%BUILDER%%", builder)
    if "%%" in page:
        raise SystemExit("a placeholder was left in the page")
    out = HERE / "fm1-rescue.html"
    out.write_text(page, encoding="utf-8")
    print("%s: %d bytes" % (out, len(page.encode())))


if __name__ == "__main__":
    main()
