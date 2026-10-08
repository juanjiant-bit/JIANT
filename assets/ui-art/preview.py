#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""test/out/*.bin (what ui_art.c sent to the LCD) -> preview/NN.png and preview/sheet.png (2x, pixel exact)."""
import sys, pathlib
import numpy as np
from PIL import Image, ImageDraw
src = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "test/out"); dst = pathlib.Path("preview"); dst.mkdir(exist_ok=True)
files = sorted(src.glob("*.bin")); n = len(files)
S = Image.new("RGB", (4 * 480 + 5 * 24, ((n + 3) // 4) * 504 + 24), (8, 8, 7))
for i, f in enumerate(files):
    a = np.frombuffer(f.read_bytes(), dtype="<u2").reshape(240, 240).astype(np.uint32)
    v = ((a >> 8) | (a << 8)) & 0xFFFF
    rgb = np.stack([((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31], -1).astype(np.uint8)
    im = Image.fromarray(rgb); im.save(dst / f"{i + 1:02d}.png")
    S.paste(im.resize((480, 480), Image.NEAREST), (24 + (i % 4) * 504, 24 + (i // 4) * 504))
S.save(dst / "sheet.png"); print(n, "previews")
